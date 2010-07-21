#include "soth/Stage.hpp"
#include <Eigen/QR>

namespace soth
{

  Stage::
  Stage( const MatrixXd & inJ, const bound_vector_t & inbounds,BaseY & inY )
    : J(inJ), bounds(inbounds)
    ,Y(inY)
    ,nr(J.rows()),nc(J.cols())
    ,W_(nr,nr),ML_(nr,nc),e_(nr)
    ,M(ML_,false),L(ML_,false),e(e_,false)
    ,isWIdenty(true),W(W_,false)
    ,Ir(L.getRowIndices()),Irn(M.getRowIndices() )
    ,sizeM(0),sizeL(0)
  {
    assert( bounds.size() == J.rows() );
  }


  void Stage::
  computeInitialCOD( const unsigned int previousRank,
		     const Indirect & initialIr )
  {
    std::cout << "J = " << (MATLAB)J << std::endl;

    if( initialIr.size()==0 )
      {
	/*TODO*/throw "TODO";
      }

    /* Compute ML=J(initIr,:)*Y. */
    for( unsigned int i=0;i<initialIr.size();++i )
      {
	MatrixXd::RowXpr MLrow = ML_.row(i);
	MLrow = J.row(initialIr(i));
	Y.applyThisOnTheLeft( MLrow );
      }
    std::cout << "ML = " << (MATLAB)ML_ << std::endl;

    /* Fix the size of M and L. L is supposed full rank yet. */
    sizeA=initialIr.size();
    sizeL=sizeA;
    sizeM=previousRank;
    std::cout << "sizesAML = [" << sizeA << ", " << sizeM << ", "
	      << sizeL << "]." << std::endl;

    // M=submatrix(ML,1:previousRank); L=submatrix(ML,previousRank+1:end);
    M.setRangeCol(0,previousRank);    M.setRangeRow(0,sizeA);
    L.setRangeCol(previousRank,nc);   L.setRangeRow(0,sizeA);
    //M.insertRow(2);
    std::cout << "M = " << (MATLAB)M << std::endl;
    std::cout << "L = " << (MATLAB)L << std::endl;

    // A=columnMajor(L)  // A==L'
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> mQR(nr,nc);

    // qr(A);
    mQR.compute(L.transpose());
    const MatrixXd & QR = mQR.matrixQR();
    std::cout << "mQR = " << (MATLAB)QR << std::endl;
    const VectorXi & P = mQR.colsPermutation().indices();
    for( MatrixXd::Index i=0;i<QR.diagonalSize();++i )
      {
	rowL0(P(i)).head(i+1) =  QR.col(i).head(i+1);
	rowL0(P(i)).tail(nc-sizeM-i-1).setZero();
      }
    L.setRowIndices(P);
    M.setRowIndices(P);
    std::cout << "L = " << (MATLAB)L << std::endl;
    std::cout << "M = " << (MATLAB)M << std::endl;

    //W.setRowIndices(Ir);
    W.setRangeRow(0,sizeL);
    W.setColIndices(Ir);
    W_.setIdentity(); // DEBUG
    //std::cout << "W = " << (MATLAB)W << std::endl;
    std::cout << "WL = " << (MATLAB)(MatrixXd)(W*L) << std::endl;

    // for i=rank:-1:1
    //   if( L(i,i)!= 0 ) break;
    // 	nullifyLineDeficient( i );
    //sizeL = mQR.rank();
    const Index rank = mQR.rank();
    while( sizeL>rank )
      {
	nullifyLineDeficient( sizeL-1,rank );
      }
    L.setRangeCol(sizeM,sizeM+sizeL);
    std::cout << "Lo = " << (MATLAB)L << std::endl;
    std::cout << "W = " << (MATLAB)W << std::endl;

    // RotationHouseHolder_list_t Yup( A );
    HouseholderSequence Yup( mQR.matrixQR(),mQR.hCoeffs(),rank );
    // Y=Y*Yup;
    Y.composeOnTheRight(Yup);

  }

  // row is the position of the line, r its length
  void Stage::
  nullifyLineDeficient( const Index row, const Index in_r )
  {
   /*
      Jd = L.row(r);
      foreach i in rank:-1:1
        if( Jd(i)==0 ) continue;
	gr= GR(L(Ir(i),i),Jd(i),i,r );
	L=gr*L;
	W=W*gr';
      Ir >> r;
      In << r;
     */
    const Index r = (in_r<0)?row-1:in_r;
    std::cout << "r = " << r << " , row = " << row << std::endl;

    if( isWIdenty )
      {
	isWIdenty = false;
	W_.setIdentity();
      }

    for( Index i=r-1;i>=0;--i )
      {
	Givensd G1;
	G1.makeGivens(L(i,i),L(row,i));
	Block<MatrixXd> ML(ML_,0,0,nr,sizeM+r);
	ML.applyOnTheLeft( Ir(i),Ir(row),G1.transpose());
	std::cout << "MLa = " << (MATLAB)ML << std::endl;
	W_.applyOnTheRight( Ir(i),Ir(row),G1);

	std::cout << "W = " << (MATLAB)W << std::endl;
	std::cout << "L = " << (MATLAB)L << std::endl;
	std::cout << "WL = " << (MATLAB)(MatrixXd)(W.transpose()*L) << std::endl;
      }

    L.removeRow(row);
    sizeL--;
  }

    /* WMLY = [ W*M W(:,1:rank)*L zeros(sizeA,nc-sizeM-sizeL) ]*Y' */
  void Stage::
   recompose( MatrixXd& WMLY )
   {
     WMLY.resize(sizeA,nc); WMLY.setZero();
     std::cout << WMLY << std::endl;
     WMLY.block(0,0,sizeA,sizeM) = W*M;
     std::cout << WMLY << std::endl;
     std::cout << "wb = " << W.block(0,0,sizeA,sizeL)*L << std::endl;
     WMLY.block(0,sizeM,sizeA,sizeL) = W.block(0,0,sizeA,sizeL)*L;
     std::cout << (MATLAB)WMLY << std::endl;

     Y.applyTransposeOnTheLeft(WMLY);
     std::cout << (MATLAB)WMLY << std::endl;
   }


  // void Stage::
  // applyLeftGiven( const Givensd& gr )
  // {
    // Apply on L.

    // Apply on M.

    // Apply on W.

  //}


  Stage::RowL Stage::rowL0( const unsigned int r )
  {
    return ML_.row(Ir(r)).tail(nc-sizeM);
  }


}; // namespace soth
