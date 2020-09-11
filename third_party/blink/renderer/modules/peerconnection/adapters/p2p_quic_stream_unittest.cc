// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quic/core/quic_data_writer.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream_delegate.h"

namespace blink {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Invoke;
using testing::InvokeWithoutArgs;

const uint32_t kDefaultStreamWriteBufferSize = 1024;
const uint32_t kDefaultStreamDelegateReadBufferSize = 1024;
const quic::QuicStreamId kStreamId = 5;
const uint8_t kSomeData[] = {'h', 'o', 'w', 'd', 'y'};
const uint8_t kMoreData[] = {'m', 'o', 'r', 'e'};

}  // namespace

// Unit tests for the P2PQuicStream, using a mock QuicSession, which allows
// us to isolate testing the behaviors of reading a writing.
class P2PQuicStreamTest : public testing::Test {
 public:
  P2PQuicStreamTest() {
    // TODO(crbug/1070747): Fix tests for IETF QUIC.
    quic::test::DisableQuicVersionsWithTls();
    connection_ = new quic::test::MockQuicConnection(
        &connection_helper_, &alarm_factory_, quic::Perspective::IS_CLIENT);
    session_ = std::make_unique<quic::test::MockQuicSession>(connection_);
    session_->Initialize();
    // DCHECKS get hit when the clock is at 0.
    connection_helper_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  ~P2PQuicStreamTest() override {}

  void InitializeStream(
      uint32_t delegate_read_buffer_size = kDefaultStreamDelegateReadBufferSize,
      uint32_t write_buffer_size = kDefaultStreamWriteBufferSize) {
    stream_ =
        new P2PQuicStreamImpl(kStreamId, session_.get(),
                              delegate_read_buffer_size, write_buffer_size);
    stream_->SetDelegate(&delegate_);
    // The session takes the ownership of the stream.
    session_->ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream_));
  }

  template <wtf_size_t Size>
  static quiche::QuicheStringPiece StringPieceFromArray(
      const uint8_t (&array)[Size]) {
    return quiche::QuicheStringPiece(reinterpret_cast<const char*>(array),
                                     Size);
  }

  template <wtf_size_t Size>
  static Vector<uint8_t> VectorFromArray(const uint8_t (&array)[Size]) {
    Vector<uint8_t> vector;
    vector.Append(array, Size);
    return vector;
  }

  quic::test::MockQuicConnectionHelper connection_helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  // Owned by the |session_|.
  quic::test::MockQuicConnection* connection_;
  // The MockQuicSession allows us to see data that is being written and control
  // whether the data is being "sent across" or blocked.
  std::unique_ptr<quic::test::MockQuicSession> session_;
  MockP2PQuicStreamDelegate delegate_;
  // Owned by |session_|.
  P2PQuicStreamImpl* stream_;
};

TEST_F(P2PQuicStreamTest, StreamSendsFinAndCanNoLongerWrite) {
  InitializeStream();
  EXPECT_CALL(*session_,
              WritevData(kStreamId, 0u, 0u, quic::StreamSendingState::FIN,
                         quic::NOT_RETRANSMISSION, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 0u, 0u,
                                     quic::StreamSendingState::FIN,
                                     quic::NOT_RETRANSMISSION, QUICHE_NULLOPT);
      }));

  stream_->WriteData({}, /*fin=*/true);

  EXPECT_TRUE(stream_->fin_sent());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_FALSE(stream_->reading_stopped());
}

TEST_F(P2PQuicStreamTest, StreamResetSendsRst) {
  InitializeStream();
  EXPECT_CALL(*session_, SendRstStream(kStreamId, _, _, _));
  stream_->Reset();
  EXPECT_TRUE(stream_->rst_sent());
}

// Tests that when a stream receives a stream frame with the FIN bit set it
// will fire the appropriate callback and close the stream for reading.
TEST_F(P2PQuicStreamTest, StreamOnStreamFrameWithFin) {
  InitializeStream();
  EXPECT_CALL(delegate_, OnDataReceived(_, /*fin=*/true));

  quic::QuicStreamFrame fin_frame(kStreamId, /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_FALSE(stream_->write_side_closed());
}

// Tests that when a stream receives a stream frame with the FIN bit set after
// it has written the FIN bit, then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterSendingThenReceivingFin) {
  InitializeStream();
  EXPECT_CALL(*session_,
              WritevData(kStreamId, 0u, 0u, _, quic::NOT_RETRANSMISSION, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 0u, 0u,
                                     quic::StreamSendingState::FIN,
                                     quic::NOT_RETRANSMISSION, QUICHE_NULLOPT);
      }));

  stream_->WriteData({}, /*fin=*/true);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream writes a FIN bit after receiving a stream frame with
// the FIN bit then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterReceivingThenSendingFin) {
  InitializeStream();
  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  EXPECT_CALL(*session_,
              WritevData(kStreamId, 0u, 0u, quic::StreamSendingState::FIN,
                         quic::NOT_RETRANSMISSION, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 0u, 0u,
                                     quic::StreamSendingState::FIN,
                                     quic::NOT_RETRANSMISSION, QUICHE_NULLOPT);
      }));

  stream_->WriteData({}, /*fin=*/true);

  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream writes some data with the FIN bit set, and receives
// data with the FIN bit set it will become closed.
TEST_F(P2PQuicStreamTest, StreamClosedAfterWritingAndReceivingDataWithFin) {
  InitializeStream();
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke(
          [this](quic::QuicStreamId id, size_t write_length,
                 quic::QuicStreamOffset offset, quic::StreamSendingState state,
                 quic::TransmissionType type,
                 quiche::QuicheOptional<quic::EncryptionLevel> level) {
            // WritevData does not pass the data. The data is saved to the
            // stream, so we must grab it before it's consumed, in order to
            // check that it's what was written.
            std::string data_consumed_by_quic(write_length, 'a');
            quic::QuicDataWriter writer(write_length, &data_consumed_by_quic[0],
                                        quiche::NETWORK_BYTE_ORDER);
            stream_->WriteStreamData(offset, write_length, &writer);

            EXPECT_THAT(data_consumed_by_quic, ElementsAreArray(kSomeData));
            EXPECT_EQ(quic::StreamSendingState::FIN, state);
            return quic::QuicConsumedData(
                write_length, state != quic::StreamSendingState::NO_FIN);
          }));

  stream_->WriteData(VectorFromArray(kSomeData),
                     /*fin=*/true);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  quic::QuicStreamFrame fin_frame_with_data(stream_->id(), /*fin=*/true, 0,
                                            StringPieceFromArray(kSomeData));
  stream_->OnStreamFrame(fin_frame_with_data);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream receives a RST_STREAM frame it will fire the
// appropriate callback and the stream will become closed.
TEST_F(P2PQuicStreamTest, StreamClosedAfterReceivingReset) {
  InitializeStream();
  EXPECT_CALL(delegate_, OnRemoteReset());

  quic::QuicRstStreamFrame rst_frame(quic::kInvalidControlFrameId, kStreamId,
                                     quic::QUIC_STREAM_CANCELLED, 0);
  if (!VersionHasIetfQuicFrames(connection_->version().transport_version)) {
    // Google RST_STREAM closes the stream in both directions. A RST_STREAM
    // is then sent to the peer to communicate the final byte offset.
    EXPECT_CALL(*session_,
                SendRstStream(kStreamId, quic::QUIC_RST_ACKNOWLEDGEMENT, 0, _));
  }
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(connection_->version().transport_version)) {
    // In IETF QUIC, the RST_STREAM only closes the stream in one direction.
    // A STOP_SENDING frame in require to induce the a RST_STREAM being
    // send to close the other direction.
    EXPECT_CALL(*connection_, SendControlFrame(_));
    EXPECT_CALL(*connection_, OnStreamReset(kStreamId, testing::_));
    quic::QuicStopSendingFrame stop_sending_frame(
        quic::kInvalidControlFrameId, kStreamId, quic::QUIC_STREAM_NO_ERROR);
    session_->OnStopSendingFrame(stop_sending_frame);
  }

  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that data written to the P2PQuicStream will appropriately get written
// to the underlying QUIC library.
TEST_F(P2PQuicStreamTest, StreamWritesData) {
  InitializeStream();
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke(
          [this](quic::QuicStreamId id, size_t write_length,
                 quic::QuicStreamOffset offset, quic::StreamSendingState state,
                 quic::TransmissionType type,
                 quiche::QuicheOptional<quic::EncryptionLevel> level) {
            // quic::QuicSession::WritevData does not pass the data. The data is
            // saved to the stream, so we must grab it before it's consumed, in
            // order to check that it's what was written.
            std::string data_consumed_by_quic(write_length, 'a');
            quic::QuicDataWriter writer(write_length, &data_consumed_by_quic[0],
                                        quiche::NETWORK_BYTE_ORDER);
            stream_->WriteStreamData(offset, write_length, &writer);

            EXPECT_THAT(data_consumed_by_quic, ElementsAreArray(kSomeData));
            EXPECT_EQ(quic::StreamSendingState::NO_FIN, state);
            return quic::QuicConsumedData(
                write_length, state != quic::StreamSendingState::NO_FIN);
          }));
  EXPECT_CALL(delegate_, OnWriteDataConsumed(base::size(kSomeData)));

  stream_->WriteData(VectorFromArray(kSomeData), /*fin=*/false);
}

// Tests that data written to the P2PQuicStream will appropriately get written
// to the underlying QUIC library with the FIN bit set.
TEST_F(P2PQuicStreamTest, StreamWritesDataWithFin) {
  InitializeStream();
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke(
          [this](quic::QuicStreamId id, size_t write_length,
                 quic::QuicStreamOffset offset, quic::StreamSendingState state,
                 quic::TransmissionType type,
                 quiche::QuicheOptional<quic::EncryptionLevel> level) {
            // WritevData does not pass the data. The data is saved to the
            // stream, so we must grab it before it's consumed, in order to
            // check that it's what was written.
            std::string data_consumed_by_quic(write_length, 'a');
            quic::QuicDataWriter writer(write_length, &data_consumed_by_quic[0],
                                        quiche::NETWORK_BYTE_ORDER);
            stream_->WriteStreamData(offset, write_length, &writer);

            EXPECT_THAT(data_consumed_by_quic, ElementsAreArray(kSomeData));
            EXPECT_EQ(quic::StreamSendingState::FIN, state);
            return quic::QuicConsumedData(
                write_length, state != quic::StreamSendingState::NO_FIN);
          }));
  EXPECT_CALL(delegate_, OnWriteDataConsumed(base::size(kSomeData)));

  stream_->WriteData(VectorFromArray(kSomeData), /*fin=*/true);
}

// Tests that when written data is not consumed by QUIC (due to buffering),
// the OnWriteDataConsumed will not get fired.
TEST_F(P2PQuicStreamTest, StreamWritesDataAndNotConsumedByQuic) {
  InitializeStream();
  EXPECT_CALL(delegate_, OnWriteDataConsumed(_)).Times(0);
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke([](quic::QuicStreamId id, size_t write_length,
                          quic::QuicStreamOffset offset,
                          quic::StreamSendingState state,
                          quic::TransmissionType type,
                          quiche::QuicheOptional<quic::EncryptionLevel> level) {
        // We mock that the QUIC library is not consuming the data, meaning it's
        // being buffered. In this case, the OnWriteDataConsumed() callback
        // should not be called.
        return quic::QuicConsumedData(/*bytes_consumed=*/0,
                                      quic::StreamSendingState::NO_FIN);
      }));

  stream_->WriteData(VectorFromArray(kSomeData), /*fin=*/true);
}

// Tests that OnWriteDataConsumed() is fired with the amount consumed by QUIC.
// This tests the case when amount consumed by QUIC is less than what is written
// with P2PQuicStream::WriteData. This can happen when QUIC is receiving back
// pressure from the receive side, and its "send window" is smaller than the
// amount attempted to be written.
TEST_F(P2PQuicStreamTest, StreamWritesDataAndPartiallyConsumedByQuic) {
  InitializeStream();
  size_t amount_consumed_by_quic = 2;
  EXPECT_CALL(delegate_, OnWriteDataConsumed(amount_consumed_by_quic));
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke(
          [&amount_consumed_by_quic](
              quic::QuicStreamId id, size_t write_length,
              quic::QuicStreamOffset offset, quic::StreamSendingState state,
              quic::TransmissionType type,
              quiche::QuicheOptional<quic::EncryptionLevel> level) {
            // We mock that the QUIC library is only consuming some of the data,
            // meaning the rest is being buffered.
            return quic::QuicConsumedData(
                /*bytes_consumed=*/amount_consumed_by_quic,
                quic::StreamSendingState::NO_FIN);
          }));

  stream_->WriteData(VectorFromArray(kSomeData), /*fin=*/true);
}

// Tests if a P2PQuicStream receives data it will appropriately fire the
// OnDataReceived callback to the delegate.
TEST_F(P2PQuicStreamTest, StreamReceivesData) {
  InitializeStream();
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));

  EXPECT_CALL(delegate_, OnDataReceived(VectorFromArray(kSomeData),
                                        /*fin=*/false));

  stream_->OnStreamFrame(stream_frame);
}

// Tests that when received data is marked consumed it is appropriately
// reflected in the P2PQuicStream's view of the delegate read buffer size.
TEST_F(P2PQuicStreamTest, MarkConsumedData) {
  InitializeStream();
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));

  EXPECT_CALL(delegate_, OnDataReceived(_, _));
  stream_->OnStreamFrame(stream_frame);
  // At this point the application has received data but not marked is as
  // consumed, so from the P2PQuicStream perspective that data has been
  // buffered.
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());

  stream_->MarkReceivedDataConsumed(base::size(kSomeData));
  EXPECT_EQ(0u, stream_->DelegateReadBufferedAmountForTesting());
}

// Tests that if the delegate's read buffer is "full" from the
// P2PQuicStream's perspective, then getting more data will not fire the
// OnDataReceived callback.
TEST_F(P2PQuicStreamTest, StreamReceivesDataWithFullReadBuffer) {
  // The P2PQuicStream is created with a delegate read buffer size equal
  // to the size of the data that is being received.
  InitializeStream(/*delegate_read_buffer_size=*/base::size(kSomeData));
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));

  EXPECT_CALL(delegate_, OnDataReceived(_, _)).Times(1);
  stream_->OnStreamFrame(stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());

  // Delegate read buffer is now full. Receiving more data should not fire the
  // callback.
  quic::QuicStreamFrame new_stream_frame(stream_->id(), /*fin=*/false,
                                         /*offset=*/base::size(kSomeData),
                                         StringPieceFromArray(kMoreData));
  stream_->OnStreamFrame(new_stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());
}

// Tests that if the delegate's read buffer is "full" from the
// P2PQuicStream's perspective, and then getting an empty STREAM frame with the
// FIN bit set, will fire Delegate::OnDataReceived with fin set to true.
TEST_F(P2PQuicStreamTest, StreamReceivesFinWithFullReadBuffer) {
  // The P2PQuicStream is created with a delegate read buffer size equal
  // to the size of the data that is being received.
  InitializeStream(/*delegate_read_buffer_size=*/base::size(kSomeData));
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));

  EXPECT_CALL(delegate_, OnDataReceived(ElementsAreArray(kSomeData),
                                        /*fin=*/false))
      .Times(1);
  stream_->OnStreamFrame(stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());

  // Delegate read buffer is now full, but because the STREAM frame with the FIN
  // bit doesn't contain any data, it means that all data has been consumed from
  // the sequencer up to the FIN bit. This fires OnDataReceived with the
  // fin=true.
  EXPECT_CALL(delegate_, OnDataReceived(_, true)).Times(1);
  quic::QuicStreamFrame new_stream_frame(stream_->id(), /*fin=*/true,
                                         /*offset=*/base::size(kSomeData), 0);
  stream_->OnStreamFrame(new_stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());
}

// Tests that when the delegate's read buffer is "full" from the
// P2PQuicStream's perspective, the Delegate::OnDataReceived callback is
// fired after the received data is marked as consumed by the delegate.
TEST_F(P2PQuicStreamTest, StreamDataConsumedWithFullDelegateReadBuffer) {
  // The P2PQuicStream is created with a delegate read buffer size equal
  // to the size of the data that is being received.
  InitializeStream(/*delegate_read_buffer_size=*/base::size(kSomeData));
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));

  EXPECT_CALL(delegate_, OnDataReceived(ElementsAreArray(kSomeData),
                                        /*fin=*/false))
      .Times(1);
  stream_->OnStreamFrame(stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());

  // Delegate read buffer is now full. Receiving more data should not fire the
  // callback.
  quic::QuicStreamFrame new_stream_frame(stream_->id(), /*fin=*/true,
                                         /*offset=*/base::size(kSomeData),
                                         StringPieceFromArray(kMoreData));
  stream_->OnStreamFrame(new_stream_frame);
  EXPECT_EQ(base::size(kSomeData),
            stream_->DelegateReadBufferedAmountForTesting());

  // Marking the original data as consumed should fire the new data to be
  // received.
  EXPECT_CALL(delegate_, OnDataReceived(ElementsAreArray(kMoreData),
                                        /*fin=*/true))
      .Times(1);
  stream_->MarkReceivedDataConsumed(base::size(kSomeData));
  EXPECT_EQ(base::size(kMoreData),
            stream_->DelegateReadBufferedAmountForTesting());
}

// Tests that when receiving more data than available in the delegate read
// buffer, that the delegate will get an OnDataReceived callback for the amount
// available in its buffer. Then later when the delegate marks the data as
// consumed it will get another OnDataReceived callback.
TEST_F(P2PQuicStreamTest, StreamReceivesMoreDataThanDelegateReadBufferSize) {
  const uint8_t kData[] = {'s', 'o', 'm', 'e', 'd', 'a', 't', 'a'};
  // The P2PQuicStream is created with a delegate read buffer size equal
  // to half of the data being sent.
  InitializeStream(/*delegate_read_buffer_size=*/4);
  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/true, 0,
                                     StringPieceFromArray(kData));

  // Upon receiving the stream frame the Delegate should receive "some", because
  // that's all it has space to buffer.
  EXPECT_CALL(delegate_, OnDataReceived(ElementsAre('s', 'o', 'm', 'e'),
                                        /*fin=*/false))
      .Times(1);
  stream_->OnStreamFrame(stream_frame);
  EXPECT_EQ(4u, stream_->DelegateReadBufferedAmountForTesting());

  // Upon consuming 2 bytes of data, the delegate should receive the next part
  // of the message - "da".
  EXPECT_CALL(delegate_, OnDataReceived(ElementsAre('d', 'a'), /*fin=*/false))
      .Times(1);
  stream_->MarkReceivedDataConsumed(2);
  EXPECT_EQ(4u, stream_->DelegateReadBufferedAmountForTesting());

  // After consuming 4 bytes of data (all received data thus far), the delegate
  // should receive the next part of the message - "ta" and the FIN bit.
  EXPECT_CALL(delegate_, OnDataReceived(ElementsAre('t', 'a'), /*fin=*/true))
      .Times(1);
  stream_->MarkReceivedDataConsumed(4);
  // Just the last data received ("ta") is held in the delegate's read buffer.
  EXPECT_EQ(2u, stream_->DelegateReadBufferedAmountForTesting());
}

// Tests that after a delegate unsets itself, it will no longer receive the
// OnWriteDataConsumed callback.
TEST_F(P2PQuicStreamTest, UnsetDelegateDoesNotFireOnWriteDataConsumed) {
  InitializeStream();
  stream_->SetDelegate(nullptr);
  // Mock out the QuicSession to get the QuicStream::OnStreamDataConsumed
  // callback to fire.
  EXPECT_CALL(*session_,
              WritevData(kStreamId,
                         /*write_length=*/base::size(kSomeData), _, _, _, _))
      .WillOnce(Invoke([](quic::QuicStreamId id, size_t write_length,
                          quic::QuicStreamOffset offset,
                          quic::StreamSendingState state,
                          quic::TransmissionType type,
                          quiche::QuicheOptional<quic::EncryptionLevel> level) {
        return quic::QuicConsumedData(
            write_length, state != quic::StreamSendingState::NO_FIN);
      }));

  EXPECT_CALL(delegate_, OnWriteDataConsumed(_)).Times(0);

  stream_->WriteData(VectorFromArray(kSomeData), /*fin=*/false);
}

// Tests that after a delegate unsets itself, it will no longer receive the
// OnRemoteReset callback.
TEST_F(P2PQuicStreamTest, UnsetDelegateDoesNotFireOnRemoteReset) {
  InitializeStream();
  stream_->SetDelegate(nullptr);
  EXPECT_CALL(delegate_, OnRemoteReset()).Times(0);

  quic::QuicRstStreamFrame rst_frame(quic::kInvalidControlFrameId, kStreamId,
                                     quic::QUIC_STREAM_CANCELLED, 0);
  stream_->OnStreamReset(rst_frame);
}

// Tests that after a delegate unsets itself, it will no longer receive the
// OnDataReceived callback when receiving a stream frame with data and no FIN
// bit.
TEST_F(P2PQuicStreamTest, UnsetDelegateDoesNotFireOnDataReceivedWithData) {
  InitializeStream();
  stream_->SetDelegate(nullptr);

  EXPECT_CALL(delegate_, OnDataReceived(_, _)).Times(0);

  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/false, 0,
                                     StringPieceFromArray(kSomeData));
  stream_->OnStreamFrame(stream_frame);
}

// Tests that after a delegate unsets itself, it will no longer receive the
// OnDataReceived callback when receiving a stream frame with the FIN bit.
TEST_F(P2PQuicStreamTest, UnsetDelegateDoesNotFireOnDataReceivedWithFin) {
  InitializeStream();
  stream_->SetDelegate(nullptr);

  EXPECT_CALL(delegate_, OnDataReceived(_, _)).Times(0);

  quic::QuicStreamFrame stream_frame(stream_->id(), /*fin=*/true, 0, {});
  stream_->OnStreamFrame(stream_frame);
}
}  // namespace blink
