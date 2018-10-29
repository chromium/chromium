// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_stream.h"

#include <memory>

#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/quic_write_blocked_list.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_flow_controller_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_sequencer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const size_t kDataLen = 9;

class TestStream : public QuicStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session, StreamType type)
      : QuicStream(id, session, /*is_static=*/false, type) {}

  void OnDataAvailable() override {}

  MOCK_METHOD0(OnCanWriteNewData, void());

  using QuicStream::CanWriteNewData;
  using QuicStream::CloseWriteSide;
  using QuicStream::fin_buffered;
  using QuicStream::OnClose;
  using QuicStream::set_ack_listener;
  using QuicStream::WriteMemSlices;
  using QuicStream::WriteOrBufferData;
  using QuicStream::WritevData;

 private:
  QuicString data_;
};

class QuicStreamTest : public QuicTestWithParam<bool> {
 public:
  QuicStreamTest()
      : initial_flow_control_window_bytes_(kMaxPacketSize),
        zero_(QuicTime::Delta::Zero()),
        supported_versions_(AllSupportedVersions()) {
  }

  void Initialize() {
    connection_ = new StrictMock<MockQuicConnection>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER,
        ParsedVersionOfIndex(supported_versions_, 0));
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    session_ = QuicMakeUnique<StrictMock<MockQuicSession>>(connection_);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(
        session_->config(), initial_flow_control_window_bytes_);

    stream_ = new TestStream(kTestStreamId, session_.get(), BIDIRECTIONAL);
    // session_ now owns stream_.
    session_->ActivateStream(QuicWrapUnique(stream_));
    // Ignore resetting when session_ is terminated.
    EXPECT_CALL(*session_, SendRstStream(kTestStreamId, _, _))
        .Times(AnyNumber());
    write_blocked_list_ =
        QuicSessionPeer::GetWriteBlockedStreams(session_.get());
  }

  bool fin_sent() { return QuicStreamPeer::FinSent(stream_); }
  bool rst_sent() { return QuicStreamPeer::RstSent(stream_); }

  void set_initial_flow_control_window_bytes(uint32_t val) {
    initial_flow_control_window_bytes_ = val;
  }

  bool HasWriteBlockedStreams() {
    return write_blocked_list_->HasWriteBlockedSpecialStream() ||
           write_blocked_list_->HasWriteBlockedDataStreams();
  }

  QuicConsumedData CloseStreamOnWriteError(QuicStream* /*stream*/,
                                           QuicStreamId id,
                                           size_t /*write_length*/,
                                           QuicStreamOffset /*offset*/,
                                           StreamSendingState /*state*/) {
    session_->CloseStream(id);
    return QuicConsumedData(1, false);
  }

  bool ClearControlFrame(const QuicFrame& frame) {
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  std::unique_ptr<MockQuicSession> session_;
  TestStream* stream_;
  QuicWriteBlockedList* write_blocked_list_;
  uint32_t initial_flow_control_window_bytes_;
  QuicTime::Delta zero_;
  ParsedQuicVersionVector supported_versions_;
  const QuicStreamId kTestStreamId =
      QuicUtils::GetHeadersStreamId(
          AllSupportedVersions()[0].transport_version) +
      2;
};

TEST_F(QuicStreamTest, WriteAllData) {
  Initialize();

  size_t length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), PACKET_8BYTE_CONNECTION_ID,
              PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
              !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(Invoke(&(MockQuicSession::ConsumeData)));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(QuicStreamTest, NoBlockingIfNoDataOrFin) {
  Initialize();

  // Write no data and no fin.  If we consume nothing we should not be write
  // blocked.
  EXPECT_QUIC_BUG(stream_->WriteOrBufferData(QuicStringPiece(), false, nullptr),
                  "");
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(QuicStreamTest, BlockIfOnlySomeDataConsumed) {
  Initialize();

  // Write some data and no fin.  If we consume some but not all of the data,
  // we should be write blocked a not all the data was consumed.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 1u, 0u,
                                            NO_FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), false, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
}

TEST_F(QuicStreamTest, BlockIfFinNotConsumedWithData) {
  Initialize();

  // Write some data and no fin.  If we consume all the data but not the fin,
  // we should be write blocked because the fin was not consumed.
  // (This should never actually happen as the fin should be sent out with the
  // last data)
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 2u, 0u,
                                            NO_FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, BlockIfSoloFinNotConsumed) {
  Initialize();

  // Write no data and a fin.  If we consume nothing we should be write blocked,
  // as the fin was not consumed.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(QuicStringPiece(), true, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, CloseOnPartialWrite) {
  Initialize();

  // Write some data and no fin. However, while writing the data
  // close the stream and verify that MarkConnectionLevelWriteBlocked does not
  // crash with an unknown stream.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(Invoke(this, &QuicStreamTest::CloseStreamOnWriteError));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), false, nullptr);
  ASSERT_EQ(0u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, WriteOrBufferData) {
  Initialize();

  EXPECT_FALSE(HasWriteBlockedStreams());
  size_t length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), PACKET_8BYTE_CONNECTION_ID,
              PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
              !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(),
                                            kDataLen - 1, 0u, NO_FIN);
      }));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_EQ(10u, stream_->BufferedDataBytes());
  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(),
                                            kDataLen - 1, kDataLen - 1, NO_FIN);
      }));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 2u,
                                            2 * kDataLen - 2, NO_FIN);
      }));
  stream_->OnCanWrite();
}

TEST_F(QuicStreamTest, WriteOrBufferDataReachStreamLimit) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();
  QuicString data("aaaaa");
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - data.length(),
                                        stream_);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(&(MockQuicSession::ConsumeData)));
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
  EXPECT_QUIC_BUG(stream_->WriteOrBufferData("a", false, nullptr),
                  "Write too many data via stream");
}

TEST_F(QuicStreamTest, ConnectionCloseAfterStreamClose) {
  Initialize();

  QuicStreamPeer::CloseReadSide(stream_);
  stream_->CloseWriteSide();
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
  stream_->OnConnectionClosed(QUIC_INTERNAL_ERROR,
                              ConnectionCloseSource::FROM_SELF);
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
}

TEST_F(QuicStreamTest, RstAlwaysSentIfNoFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if no FIN has been sent, we send a RST.

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with no FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 1u, 0u,
                                            NO_FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 1), false, nullptr);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we send a RST.
  EXPECT_CALL(*session_, SendRstStream(_, _, _));
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(QuicStreamTest, RstNotSentIfFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a FIN has been sent, we don't also send a RST.

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 1u, 0u,
                                            FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 1), true, nullptr);
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we do not send a RST.
  stream_->OnClose();
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());
}

TEST_F(QuicStreamTest, OnlySendOneRst) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a stream sends a RST, it doesn't send an additional RST during
  // OnClose() (this shouldn't be harmful, but we shouldn't do it anyway...)

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Reset the stream.
  const int expected_resets = 1;
  EXPECT_CALL(*session_, SendRstStream(_, _, _)).Times(expected_resets);
  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());

  // Now close the stream (any further resets being sent would break the
  // expectation above).
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(QuicStreamTest, StreamFlowControlMultipleWindowUpdates) {
  set_initial_flow_control_window_bytes(1000);

  Initialize();

  // If we receive multiple WINDOW_UPDATES (potentially out of order), then we
  // want to make sure we latch the largest offset we see.

  // Initially should be default.
  EXPECT_EQ(
      initial_flow_control_window_bytes_,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Check a single WINDOW_UPDATE results in correct offset.
  QuicWindowUpdateFrame window_update_1(kInvalidControlFrameId, stream_->id(),
                                        1234);
  stream_->OnWindowUpdateFrame(window_update_1);
  EXPECT_EQ(
      window_update_1.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Now send a few more WINDOW_UPDATES and make sure that only the largest is
  // remembered.
  QuicWindowUpdateFrame window_update_2(kInvalidControlFrameId, stream_->id(),
                                        1);
  QuicWindowUpdateFrame window_update_3(kInvalidControlFrameId, stream_->id(),
                                        9999);
  QuicWindowUpdateFrame window_update_4(kInvalidControlFrameId, stream_->id(),
                                        5678);
  stream_->OnWindowUpdateFrame(window_update_2);
  stream_->OnWindowUpdateFrame(window_update_3);
  stream_->OnWindowUpdateFrame(window_update_4);
  EXPECT_EQ(
      window_update_3.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));
}

TEST_F(QuicStreamTest, FrameStats) {
  Initialize();

  EXPECT_EQ(0, stream_->num_frames_received());
  EXPECT_EQ(0, stream_->num_duplicate_frames_received());
  QuicStreamFrame frame(stream_->id(), false, 0, QuicStringPiece("."));
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(1, stream_->num_frames_received());
  EXPECT_EQ(0, stream_->num_duplicate_frames_received());
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(2, stream_->num_frames_received());
  EXPECT_EQ(1, stream_->num_duplicate_frames_received());
}

// Verify that when we receive a packet which violates flow control (i.e. sends
// too much data on the stream) that the stream sequencer never sees this frame,
// as we check for violation and close the connection early.
TEST_F(QuicStreamTest, StreamSequencerNeverSeesPacketsViolatingFlowControl) {
  Initialize();

  // Receive a stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicStreamFrame frame(stream_->id(), false,
                        kInitialSessionFlowControlWindowForTest + 1,
                        QuicStringPiece("."));
  EXPECT_GT(frame.offset, QuicFlowControllerPeer::ReceiveWindowOffset(
                              stream_->flow_controller()));

  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamFrame(frame);
}

// Verify that after the consumer calls StopReading(), the stream still sends
// flow control updates.
TEST_F(QuicStreamTest, StopReadingSendsFlowControl) {
  Initialize();

  stream_->StopReading();

  // Connection should not get terminated due to flow control errors.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _))
      .Times(0);
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(this, &QuicStreamTest::ClearControlFrame));

  QuicString data(1000, 'x');
  for (QuicStreamOffset offset = 0;
       offset < 2 * kInitialStreamFlowControlWindowForTest;
       offset += data.length()) {
    QuicStreamFrame frame(stream_->id(), false, offset, data);
    stream_->OnStreamFrame(frame);
  }
  EXPECT_LT(
      kInitialStreamFlowControlWindowForTest,
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller()));
}

TEST_F(QuicStreamTest, FinalByteOffsetFromFin) {
  Initialize();

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_no_fin(stream_->id(), false, 1234,
                                      QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_no_fin);
  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, FinalByteOffsetFromRst) {
  Initialize();

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, InvalidFinalByteOffsetFromRst) {
  Initialize();

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 0xFFFFFFFFFFFF);
  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
  stream_->OnClose();
}

TEST_F(QuicStreamTest, FinalByteOffsetFromZeroLengthStreamFrame) {
  // When receiving Trailers, an empty stream frame is created with the FIN set,
  // and is passed to OnStreamFrame. The Trailers may be sent in advance of
  // queued body bytes being sent, and thus the final byte offset may exceed
  // current flow control limits. Flow control should only be concerned with
  // data that has actually been sent/received, so verify that flow control
  // ignores such a stream frame.
  Initialize();

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  const QuicStreamOffset kByteOffsetExceedingFlowControlWindow =
      kInitialSessionFlowControlWindowForTest + 1;
  const QuicStreamOffset current_stream_flow_control_offset =
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller());
  const QuicStreamOffset current_connection_flow_control_offset =
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller());
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_stream_flow_control_offset);
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_connection_flow_control_offset);
  QuicStreamFrame zero_length_stream_frame_with_fin(
      stream_->id(), /*fin=*/true, kByteOffsetExceedingFlowControlWindow,
      QuicStringPiece());
  EXPECT_EQ(0, zero_length_stream_frame_with_fin.data_length);

  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  stream_->OnStreamFrame(zero_length_stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());

  // The flow control receive offset values should not have changed.
  EXPECT_EQ(
      current_stream_flow_control_offset,
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller()));
  EXPECT_EQ(
      current_connection_flow_control_offset,
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller()));
}

TEST_F(QuicStreamTest, OnStreamResetOffsetOverflow) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, kMaxStreamLength + 1);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
  stream_->OnStreamReset(rst_frame);
}

TEST_F(QuicStreamTest, OnStreamFrameUpperLimit) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();

  // Modify receive window offset and sequencer buffer total_bytes_read_ to
  // avoid flow control violation.
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kMaxStreamLength + 5u);
  QuicFlowControllerPeer::SetReceiveWindowOffset(session_->flow_controller(),
                                                 kMaxStreamLength + 5u);
  QuicStreamSequencerPeer::SetFrameBufferTotalBytesRead(
      QuicStreamPeer::sequencer(stream_), kMaxStreamLength - 10u);

  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _))
      .Times(0);
  QuicStreamFrame stream_frame(stream_->id(), false, kMaxStreamLength - 1,
                               QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame);
  QuicStreamFrame stream_frame2(stream_->id(), true, kMaxStreamLength,
                                QuicStringPiece(""));
  stream_->OnStreamFrame(stream_frame2);
}

TEST_F(QuicStreamTest, StreamTooLong) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _))
      .Times(1);
  QuicStreamFrame stream_frame(stream_->id(), false, kMaxStreamLength,
                               QuicStringPiece("."));
  EXPECT_QUIC_PEER_BUG(stream_->OnStreamFrame(stream_frame),
                       "Receive stream frame reaches max stream length");
}

TEST_F(QuicStreamTest, SetDrainingIncomingOutgoing) {
  // Don't have incoming data consumed.
  Initialize();

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 2u, 0u,
                                            FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, QuicSessionPeer::GetDrainingStreams(session_.get())
                    ->count(kTestStreamId));
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
}

TEST_F(QuicStreamTest, SetDrainingOutgoingIncoming) {
  // Don't have incoming data consumed.
  Initialize();

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 2u, 0u,
                                            FIN);
      }));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, QuicSessionPeer::GetDrainingStreams(session_.get())
                    ->count(kTestStreamId));
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
}

TEST_F(QuicStreamTest, EarlyResponseFinHandling) {
  // Verify that if the server completes the response before reading the end of
  // the request, the received FIN is recorded.

  Initialize();
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));

  // Receive data for the request.
  QuicStreamFrame frame1(stream_->id(), false, 0, QuicStringPiece("Start"));
  stream_->OnStreamFrame(frame1);
  // When QuicSimpleServerStream sends the response, it calls
  // QuicStream::CloseReadSide() first.
  QuicStreamPeer::CloseReadSide(stream_);
  // Send data and FIN for the response.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  // Receive remaining data and FIN for the request.
  QuicStreamFrame frame2(stream_->id(), true, 0, QuicStringPiece("End"));
  stream_->OnStreamFrame(frame2);
  EXPECT_TRUE(stream_->fin_received());
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, StreamWaitsForAcks) {
  Initialize();
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  // Stream is not waiting for acks initially.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData1.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero()));
  // Stream is not waiting for acks as all sent data is acked.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData2.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Send FIN.
  stream_->WriteOrBufferData("", true, nullptr);
  // Fin only frame is not stored in send buffer.
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // kData2 is retransmitted.
  EXPECT_CALL(*mock_ack_listener, OnPacketRetransmitted(9));
  stream_->OnStreamFrameRetransmitted(9, 9, false);

  // kData2 is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero()));
  // Stream is waiting for acks as FIN is not acked.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // FIN is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(0, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(18, 0, true, QuicTime::Delta::Zero()));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, StreamDataGetAckedOutOfOrder) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  // Send data.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData("", true, nullptr);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(18, 9, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  // FIN is not acked yet.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(27, 0, true, QuicTime::Delta::Zero()));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, CancelStream) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Cancel stream.
  stream_->Reset(QUIC_STREAM_NO_ERROR);
  // stream still waits for acks as the error code is QUIC_STREAM_NO_ERROR, and
  // data is going to be retransmitted.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_STREAM_CANCELLED, 9));
  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as data is not going to be retransmitted.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, RstFrameReceivedStreamNotFinishSending) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // RST_STREAM received.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 9);
  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_RST_ACKNOWLEDGEMENT, 9));
  stream_->OnStreamReset(rst_frame);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as it does not finish sending and rst is
  // sent.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, RstFrameReceivedStreamFinishSending) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, true, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // RST_STREAM received.
  EXPECT_CALL(*session_, SendRstStream(_, _, _)).Times(0);
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  // Stream still waits for acks as it finishes sending and has unacked data.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, ConnectionClosed) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_RST_ACKNOWLEDGEMENT, 9));
  stream_->OnConnectionClosed(QUIC_INTERNAL_ERROR,
                              ConnectionCloseSource::FROM_SELF);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as connection is going to close.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, WriteBufferedData) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(&FLAGS_quic_buffered_data_threshold, 100);
  // Do not stream level flow control block this stream.
  set_initial_flow_control_window_bytes(500000);

  Initialize();
  QuicString data(1024, 'a');
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing WriteOrBufferData.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 100u, 0u,
                                            NO_FIN);
      }));
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  // Verify all data is saved.
  EXPECT_EQ(3 * data.length() - 100, stream_->BufferedDataBytes());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 100, 100u,
                                            NO_FIN);
      }));
  // Buffered data size > threshold, do not ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  stream_->OnCanWrite();
  EXPECT_EQ(3 * data.length() - 200, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  // Send buffered data to make buffered data size < threshold.
  size_t data_to_write = 3 * data.length() - 200 -
                         GetQuicFlag(FLAGS_quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(),
                                            data_to_write, 200u, NO_FIN);
      }));
  // Buffered data size < threshold, ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(0u, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing Writev.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  struct iovec iov = {const_cast<char*>(data.data()), data.length()};
  QuicConsumedData consumed = stream_->WritevData(&iov, 1, false);
  // There is no buffered data before, all data should be consumed without
  // respecting buffered data upper limit.
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(0);
  consumed = stream_->WritevData(&iov, 1, false);
  // No Data can be consumed as buffered data is beyond upper limit.
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());

  data_to_write =
      data.length() - GetQuicFlag(FLAGS_quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(),
                                            data_to_write, 0u, NO_FIN);
      }));

  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(0);
  // All data can be consumed as buffered data is below upper limit.
  consumed = stream_->WritevData(&iov, 1, false);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length() + GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());
}

TEST_F(QuicStreamTest, WritevDataReachStreamLimit) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();
  QuicString data("aaaaa");
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - data.length(),
                                        stream_);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(&(MockQuicSession::ConsumeData)));
  struct iovec iov = {const_cast<char*>(data.data()), 5u};
  QuicConsumedData consumed = stream_->WritevData(&iov, 1u, false);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  struct iovec iov2 = {const_cast<char*>(data.data()), 1u};
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
  EXPECT_QUIC_BUG(stream_->WritevData(&iov2, 1u, false),
                  "Write too many data via stream");
}

TEST_F(QuicStreamTest, WriteMemSlices) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(&FLAGS_quic_buffered_data_threshold, 100);
  // Do not flow control block this stream.
  set_initial_flow_control_window_bytes(500000);

  Initialize();
  char data[1024];
  std::vector<std::pair<char*, size_t>> buffers;
  buffers.push_back(std::make_pair(data, QUIC_ARRAYSIZE(data)));
  buffers.push_back(std::make_pair(data, QUIC_ARRAYSIZE(data)));
  QuicTestMemSliceVector vector1(buffers);
  QuicTestMemSliceVector vector2(buffers);
  QuicMemSliceSpan span1 = vector1.span();
  QuicMemSliceSpan span2 = vector2.span();

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 100u, 0u,
                                            NO_FIN);
      }));
  // There is no buffered data before, all data should be consumed.
  QuicConsumedData consumed = stream_->WriteMemSlices(span1, false);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * QUIC_ARRAYSIZE(data) - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(0);
  // No Data can be consumed as buffered data is beyond upper limit.
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * QUIC_ARRAYSIZE(data) - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  size_t data_to_write = 2 * QUIC_ARRAYSIZE(data) - 100 -
                         GetQuicFlag(FLAGS_quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(),
                                            data_to_write, 100u, NO_FIN);
      }));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  // Try to write slices2 again.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(0);
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_EQ(2 * QUIC_ARRAYSIZE(data) +
                GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->fin_buffered());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicStreamTest, WriteMemSlicesReachStreamLimit) {
  SetQuicReloadableFlag(quic_stream_too_long, true);
  Initialize();
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - 5u, stream_);
  char data[5];
  std::vector<std::pair<char*, size_t>> buffers;
  buffers.push_back(std::make_pair(data, QUIC_ARRAYSIZE(data)));
  QuicTestMemSliceVector vector1(buffers);
  QuicMemSliceSpan span1 = vector1.span();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 5u, 0u,
                                            NO_FIN);
      }));
  // There is no buffered data before, all data should be consumed.
  QuicConsumedData consumed = stream_->WriteMemSlices(span1, false);
  EXPECT_EQ(5u, consumed.bytes_consumed);

  std::vector<std::pair<char*, size_t>> buffers2;
  buffers2.push_back(std::make_pair(data, 1u));
  QuicTestMemSliceVector vector2(buffers);
  QuicMemSliceSpan span2 = vector2.span();
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
  EXPECT_QUIC_BUG(stream_->WriteMemSlices(span2, false),
                  "Write too many data via stream");
}

TEST_F(QuicStreamTest, StreamDataGetAckedMultipleTimes) {
  Initialize();
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  // Send [0, 27) and fin.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, true, nullptr);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Ack [0, 9), [5, 22) and [18, 26)
  // Verify [0, 9) 9 bytes are acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(2u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [9, 22) 13 bytes are acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(13, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(5, 17, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [22, 26) 4 bytes are acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(4, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(18, 8, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Ack [0, 27).
  // Verify [26, 27) 1 byte is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(1, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(26, 1, false, QuicTime::Delta::Zero()));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Ack Fin. Verify OnPacketAcked is called.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(0, _));
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(27, 0, true, QuicTime::Delta::Zero()));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());

  // Ack [10, 27) and fin.
  // No new data is acked, verify OnPacketAcked is not called.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(_, _)).Times(0);
  EXPECT_FALSE(
      stream_->OnStreamFrameAcked(10, 17, true, QuicTime::Delta::Zero()));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, OnStreamFrameLost) {
  Initialize();

  // Send [0, 9).
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->IsStreamFrameOutstanding(0, 9, false));

  // Try to send [9, 27), but connection is blocked.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(kData2, false, nullptr);
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_TRUE(stream_->HasBufferedData());
  EXPECT_FALSE(stream_->HasPendingRetransmission());

  // Lost [0, 9). When stream gets a chance to write, only lost data is
  // transmitted.
  stream_->OnStreamFrameLost(0, 9, false);
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  EXPECT_TRUE(stream_->HasBufferedData());

  // This OnCanWrite causes [9, 27) to be sent.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasBufferedData());

  // Send a fin only frame.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData("", true, nullptr);

  // Lost [9, 27) and fin.
  stream_->OnStreamFrameLost(9, 18, false);
  stream_->OnStreamFrameLost(27, 0, true);
  EXPECT_TRUE(stream_->HasPendingRetransmission());

  // Ack [9, 18).
  EXPECT_TRUE(
      stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero()));
  EXPECT_FALSE(stream_->IsStreamFrameOutstanding(9, 3, false));
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  // This OnCanWrite causes [18, 27) and fin to be retransmitted. Verify fin can
  // be bundled with data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 9u, 18u,
                                            FIN);
      }));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  // Lost [9, 18) again, but it is not considered as lost because kData2
  // has been acked.
  stream_->OnStreamFrameLost(9, 9, false);
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  EXPECT_TRUE(stream_->IsStreamFrameOutstanding(27, 0, true));
}

TEST_F(QuicStreamTest, CannotBundleLostFin) {
  Initialize();

  // Send [0, 18) and fin.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData2, true, nullptr);

  // Lost [0, 9) and fin.
  stream_->OnStreamFrameLost(0, 9, false);
  stream_->OnStreamFrameLost(18, 0, true);

  // Retransmit lost data. Verify [0, 9) and fin are retransmitted in two
  // frames.
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 9u, 0u,
                                            NO_FIN);
      }));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, true)));
  stream_->OnCanWrite();
}

TEST_F(QuicStreamTest, MarkConnectionLevelWriteBlockedOnWindowUpdateFrame) {
  // Set a small initial control window size.
  set_initial_flow_control_window_bytes(100);
  Initialize();

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(this, &QuicStreamTest::ClearControlFrame));
  QuicString data(1024, '.');
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());

  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream_->id(),
                                      1234);

  stream_->OnWindowUpdateFrame(window_update);
  // Verify stream is marked connection level write blocked.
  EXPECT_TRUE(HasWriteBlockedStreams());
  EXPECT_TRUE(stream_->HasBufferedData());
}

// Regression test for b/73282665.
TEST_F(QuicStreamTest,
       MarkConnectionLevelWriteBlockedOnWindowUpdateFrameWithNoBufferedData) {
  // Set a small initial flow control window size.
  const uint32_t kSmallWindow = 100;
  set_initial_flow_control_window_bytes(kSmallWindow);
  Initialize();

  QuicString data(kSmallWindow, '.');
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(Invoke(this, &QuicStreamTest::ClearControlFrame));
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());

  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream_->id(),
                                      120);
  stream_->OnWindowUpdateFrame(window_update);
  EXPECT_FALSE(stream_->HasBufferedData());
  // Verify stream is marked as blocked although there is no buffered data.
  EXPECT_TRUE(HasWriteBlockedStreams());
}

TEST_F(QuicStreamTest, RetransmitStreamData) {
  Initialize();
  InSequence s;

  // Send [0, 18) with fin.
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, true, nullptr);
  // Ack [10, 13).
  stream_->OnStreamFrameAcked(10, 3, false, QuicTime::Delta::Zero());

  // Retransmit [0, 18) with fin, and only [0, 8) is consumed.
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 10, 0, NO_FIN))
      .WillOnce(InvokeWithoutArgs([this]() {
        return MockQuicSession::ConsumeData(stream_, stream_->id(), 8, 0u,
                                            NO_FIN);
      }));
  EXPECT_FALSE(stream_->RetransmitStreamData(0, 18, true));

  // Retransmit [0, 18) with fin, and all is consumed.
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 10, 0, NO_FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 5, 13, FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 18, true));

  // Retransmit [0, 8) with fin, and all is consumed.
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 8, 0, NO_FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 0, 18, FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 8, true));
}

TEST_F(QuicStreamTest, ResetStreamOnTtlExpiresRetransmitLostData) {
  Initialize();

  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 200, 0, FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  QuicString body(200, 'a');
  stream_->WriteOrBufferData(body, true, nullptr);

  // Set TTL to be 1 s.
  QuicTime::Delta ttl = QuicTime::Delta::FromSeconds(1);
  ASSERT_TRUE(stream_->MaybeSetTtl(ttl));
  // Verify data gets retransmitted because TTL does not expire.
  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 100, 0, NO_FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 100, false));
  stream_->OnStreamFrameLost(100, 100, true);
  EXPECT_TRUE(stream_->HasPendingRetransmission());

  connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  // Verify stream gets reset because TTL expires.
  EXPECT_CALL(*session_, SendRstStream(_, QUIC_STREAM_TTL_EXPIRED, _)).Times(1);
  stream_->OnCanWrite();
}

TEST_F(QuicStreamTest, ResetStreamOnTtlExpiresEarlyRetransmitData) {
  Initialize();

  EXPECT_CALL(*session_, WritevData(_, stream_->id(), 200, 0, FIN))
      .WillOnce(Invoke(MockQuicSession::ConsumeData));
  QuicString body(200, 'a');
  stream_->WriteOrBufferData(body, true, nullptr);

  // Set TTL to be 1 s.
  QuicTime::Delta ttl = QuicTime::Delta::FromSeconds(1);
  ASSERT_TRUE(stream_->MaybeSetTtl(ttl));

  connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  // Verify stream gets reset because TTL expires.
  EXPECT_CALL(*session_, SendRstStream(_, QUIC_STREAM_TTL_EXPIRED, _)).Times(1);
  stream_->RetransmitStreamData(0, 100, false);
}

}  // namespace
}  // namespace test
}  // namespace quic
