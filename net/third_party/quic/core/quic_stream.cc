// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_stream.h"

#include "net/third_party/quic/core/quic_flow_controller.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"

using spdy::SpdyPriority;

namespace quic {

#define ENDPOINT \
  (perspective_ == Perspective::IS_SERVER ? "Server: " : "Client: ")

namespace {

struct iovec MakeIovec(QuicStringPiece data) {
  struct iovec iov = {const_cast<char*>(data.data()),
                      static_cast<size_t>(data.size())};
  return iov;
}

size_t GetInitialStreamFlowControlWindowToSend(QuicSession* session) {
  return session->config()->GetInitialStreamFlowControlWindowToSend();
}

size_t GetReceivedFlowControlWindow(QuicSession* session) {
  if (session->config()->HasReceivedInitialStreamFlowControlWindowBytes()) {
    return session->config()->ReceivedInitialStreamFlowControlWindowBytes();
  }

  return kMinimumFlowControlSendWindow;
}

}  // namespace

// static
const SpdyPriority QuicStream::kDefaultPriority;

QuicStream::QuicStream(QuicStreamId id,
                       QuicSession* session,
                       bool is_static,
                       StreamType type)
    : sequencer_(this),
      id_(id),
      session_(session),
      priority_(kDefaultPriority),
      stream_bytes_read_(0),
      stream_error_(QUIC_STREAM_NO_ERROR),
      connection_error_(QUIC_NO_ERROR),
      read_side_closed_(false),
      write_side_closed_(false),
      fin_buffered_(false),
      fin_sent_(false),
      fin_outstanding_(false),
      fin_lost_(false),
      fin_received_(false),
      rst_sent_(false),
      rst_received_(false),
      perspective_(session_->perspective()),
      flow_controller_(session_,
                       session_->connection(),
                       id_,
                       perspective_,
                       GetReceivedFlowControlWindow(session),
                       GetInitialStreamFlowControlWindowToSend(session),
                       session_->flow_controller()->auto_tune_receive_window(),
                       session_->flow_controller()),
      connection_flow_controller_(session_->flow_controller()),
      stream_contributes_to_connection_flow_control_(true),
      busy_counter_(0),
      add_random_padding_after_fin_(false),
      ack_listener_(nullptr),
      send_buffer_(
          session->connection()->helper()->GetStreamSendBufferAllocator()),
      buffered_data_threshold_(GetQuicFlag(FLAGS_quic_buffered_data_threshold)),
      is_static_(is_static),
      deadline_(QuicTime::Zero()),
      type_(type) {
  if (type_ == WRITE_UNIDIRECTIONAL) {
    set_fin_received(true);
    CloseReadSide();
  } else if (type_ == READ_UNIDIRECTIONAL) {
    set_fin_sent(true);
    CloseWriteSide();
  }
  SetFromConfig();
  session_->RegisterStreamPriority(id, is_static_, priority_);
}

QuicStream::~QuicStream() {
  if (session_ != nullptr && IsWaitingForAcks()) {
    QUIC_DVLOG(1)
        << ENDPOINT << "Stream " << id_
        << " gets destroyed while waiting for acks. stream_bytes_outstanding = "
        << send_buffer_.stream_bytes_outstanding()
        << ", fin_outstanding: " << fin_outstanding_;
  }
  if (session_ != nullptr) {
    session_->UnregisterStreamPriority(id(), is_static_);
  }
}

void QuicStream::SetFromConfig() {}

void QuicStream::OnStreamFrame(const QuicStreamFrame& frame) {
  DCHECK_EQ(frame.stream_id, id_);

  DCHECK(!(read_side_closed_ && write_side_closed_));

  if (type_ == WRITE_UNIDIRECTIONAL) {
    CloseConnectionWithDetails(
        QUIC_DATA_RECEIVED_ON_WRITE_UNIDIRECTIONAL_STREAM,
        "Data received on write unidirectional stream");
    return;
  }

  bool is_stream_too_long =
      (frame.offset > kMaxStreamLength) ||
      (kMaxStreamLength - frame.offset < frame.data_length);
  if (GetQuicReloadableFlag(quic_stream_too_long) && is_stream_too_long) {
    // Close connection if stream becomes too long.
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_stream_too_long, 4, 5);
    QUIC_PEER_BUG
        << "Receive stream frame reaches max stream length. frame offset "
        << frame.offset << " length " << frame.data_length;
    CloseConnectionWithDetails(
        QUIC_STREAM_LENGTH_OVERFLOW,
        "Peer sends more data than allowed on this stream.");
    return;
  }
  if (frame.fin) {
    fin_received_ = true;
    if (fin_sent_) {
      session_->StreamDraining(id_);
    }
  }

  if (read_side_closed_) {
    QUIC_DLOG(INFO)
        << ENDPOINT << "Stream " << frame.stream_id
        << " is closed for reading. Ignoring newly received stream data.";
    // The subclass does not want to read data:  blackhole the data.
    return;
  }

  // This count includes duplicate data received.
  size_t frame_payload_size = frame.data_length;
  stream_bytes_read_ += frame_payload_size;

  // Flow control is interested in tracking highest received offset.
  // Only interested in received frames that carry data.
  if (frame_payload_size > 0 &&
      MaybeIncreaseHighestReceivedOffset(frame.offset + frame_payload_size)) {
    // As the highest received offset has changed, check to see if this is a
    // violation of flow control.
    if (flow_controller_.FlowControlViolation() ||
        connection_flow_controller_->FlowControlViolation()) {
      CloseConnectionWithDetails(
          QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA,
          "Flow control violation after increasing offset");
      return;
    }
  }

  sequencer_.OnStreamFrame(frame);
}

int QuicStream::num_frames_received() const {
  return sequencer_.num_frames_received();
}

int QuicStream::num_duplicate_frames_received() const {
  return sequencer_.num_duplicate_frames_received();
}

void QuicStream::OnStreamReset(const QuicRstStreamFrame& frame) {
  rst_received_ = true;
  if (GetQuicReloadableFlag(quic_stream_too_long) &&
      frame.byte_offset > kMaxStreamLength) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_stream_too_long, 5, 5);
    // Peer are not suppose to write bytes more than maxium allowed.
    CloseConnectionWithDetails(QUIC_STREAM_LENGTH_OVERFLOW,
                               "Reset frame stream offset overflow.");
    return;
  }
  MaybeIncreaseHighestReceivedOffset(frame.byte_offset);
  if (flow_controller_.FlowControlViolation() ||
      connection_flow_controller_->FlowControlViolation()) {
    CloseConnectionWithDetails(
        QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA,
        "Flow control violation after increasing offset");
    return;
  }

  stream_error_ = frame.error_code;
  CloseWriteSide();
  CloseReadSide();
}

void QuicStream::OnConnectionClosed(QuicErrorCode error,
                                    ConnectionCloseSource /*source*/) {
  if (read_side_closed_ && write_side_closed_) {
    return;
  }
  if (error != QUIC_NO_ERROR) {
    stream_error_ = QUIC_STREAM_CONNECTION_ERROR;
    connection_error_ = error;
  }

  CloseWriteSide();
  CloseReadSide();
}

void QuicStream::OnFinRead() {
  DCHECK(sequencer_.IsClosed());
  // OnFinRead can be called due to a FIN flag in a headers block, so there may
  // have been no OnStreamFrame call with a FIN in the frame.
  fin_received_ = true;
  // If fin_sent_ is true, then CloseWriteSide has already been called, and the
  // stream will be destroyed by CloseReadSide, so don't need to call
  // StreamDraining.
  CloseReadSide();
}

void QuicStream::Reset(QuicRstStreamErrorCode error) {
  stream_error_ = error;
  // Sending a RstStream results in calling CloseStream.
  session()->SendRstStream(id(), error, stream_bytes_written());
  rst_sent_ = true;
}

void QuicStream::CloseConnectionWithDetails(QuicErrorCode error,
                                            const QuicString& details) {
  session()->connection()->CloseConnection(
      error, details, ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

SpdyPriority QuicStream::priority() const {
  return priority_;
}

void QuicStream::SetPriority(SpdyPriority priority) {
  priority_ = priority;
  session_->UpdateStreamPriority(id(), priority);
}

void QuicStream::WriteOrBufferData(
    QuicStringPiece data,
    bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (data.empty() && !fin) {
    QUIC_BUG << "data.empty() && !fin";
    return;
  }

  if (fin_buffered_) {
    QUIC_BUG << "Fin already buffered";
    return;
  }
  if (write_side_closed_) {
    QUIC_DLOG(ERROR) << ENDPOINT
                     << "Attempt to write when the write side is closed";
    if (type_ == READ_UNIDIRECTIONAL) {
      CloseConnectionWithDetails(
          QUIC_TRY_TO_WRITE_DATA_ON_READ_UNIDIRECTIONAL_STREAM,
          "Try to send data on read unidirectional stream");
    }
    return;
  }

  QuicConsumedData consumed_data(0, false);
  fin_buffered_ = fin;

  bool had_buffered_data = HasBufferedData();
  // Do not respect buffered data upper limit as WriteOrBufferData guarantees
  // all data to be consumed.
  if (data.length() > 0) {
    struct iovec iov(MakeIovec(data));
    QuicStreamOffset offset = send_buffer_.stream_offset();
    if (GetQuicReloadableFlag(quic_stream_too_long) &&
        kMaxStreamLength - offset < data.length()) {
      QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_stream_too_long, 1, 5);
      QUIC_BUG << "Write too many data via stream " << id_;
      CloseConnectionWithDetails(
          QUIC_STREAM_LENGTH_OVERFLOW,
          QuicStrCat("Write too many data via stream ", id_));
      return;
    }
    send_buffer_.SaveStreamData(&iov, 1, 0, data.length());
    OnDataBuffered(offset, data.length(), ack_listener);
  }
  if (!had_buffered_data && (HasBufferedData() || fin_buffered_)) {
    // Write data if there is no buffered data before.
    WriteBufferedData();
  }
}

void QuicStream::OnCanWrite() {
  if (HasDeadlinePassed()) {
    OnDeadlinePassed();
    return;
  }
  if (HasPendingRetransmission()) {
    WritePendingRetransmission();
    // Exit early to allow other streams to write pending retransmissions if
    // any.
    return;
  }

  if (write_side_closed_) {
    QUIC_DLOG(ERROR)
        << ENDPOINT << "Stream " << id()
        << " attempting to write new data when the write side is closed";
    return;
  }
  if (HasBufferedData() || (fin_buffered_ && !fin_sent_)) {
    WriteBufferedData();
  }
  if (!fin_buffered_ && !fin_sent_ && CanWriteNewData()) {
    // Notify upper layer to write new data when buffered data size is below
    // low water mark.
    OnCanWriteNewData();
  }
}

void QuicStream::MaybeSendBlocked() {
  flow_controller_.MaybeSendBlocked();
  if (!stream_contributes_to_connection_flow_control_) {
    return;
  }
  connection_flow_controller_->MaybeSendBlocked();
  // If the stream is blocked by connection-level flow control but not by
  // stream-level flow control, add the stream to the write blocked list so that
  // the stream will be given a chance to write when a connection-level
  // WINDOW_UPDATE arrives.
  if (connection_flow_controller_->IsBlocked() &&
      !flow_controller_.IsBlocked()) {
    session_->MarkConnectionLevelWriteBlocked(id());
  }
}

QuicConsumedData QuicStream::WritevData(const struct iovec* iov,
                                        int iov_count,
                                        bool fin) {
  if (write_side_closed_) {
    QUIC_DLOG(ERROR) << ENDPOINT << "Stream " << id()
                     << "attempting to write when the write side is closed";
    if (type_ == READ_UNIDIRECTIONAL) {
      CloseConnectionWithDetails(
          QUIC_TRY_TO_WRITE_DATA_ON_READ_UNIDIRECTIONAL_STREAM,
          "Try to send data on read unidirectional stream");
    }
    return QuicConsumedData(0, false);
  }

  // How much data was provided.
  size_t write_length = 0;
  if (iov != nullptr) {
    for (int i = 0; i < iov_count; ++i) {
      write_length += iov[i].iov_len;
    }
  }

  QuicConsumedData consumed_data(0, false);
  if (fin_buffered_) {
    QUIC_BUG << "Fin already buffered";
    return consumed_data;
  }

  if (GetQuicReloadableFlag(quic_stream_too_long) &&
      kMaxStreamLength - send_buffer_.stream_offset() < write_length) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_stream_too_long, 2, 5);
    QUIC_BUG << "Write too many data via stream " << id_;
    CloseConnectionWithDetails(
        QUIC_STREAM_LENGTH_OVERFLOW,
        QuicStrCat("Write too many data via stream ", id_));
    return consumed_data;
  }

  bool had_buffered_data = HasBufferedData();
  if (CanWriteNewData()) {
    // Save all data if buffered data size is below low water mark.
    consumed_data.bytes_consumed = write_length;
    if (consumed_data.bytes_consumed > 0) {
      QuicStreamOffset offset = send_buffer_.stream_offset();
      send_buffer_.SaveStreamData(iov, iov_count, 0, write_length);
      OnDataBuffered(offset, write_length, nullptr);
    }
  }
  consumed_data.fin_consumed =
      consumed_data.bytes_consumed == write_length && fin;
  fin_buffered_ = consumed_data.fin_consumed;

  if (!had_buffered_data && (HasBufferedData() || fin_buffered_)) {
    // Write data if there is no buffered data before.
    WriteBufferedData();
  }

  return consumed_data;
}

QuicConsumedData QuicStream::WriteMemSlices(QuicMemSliceSpan span, bool fin) {
  QuicConsumedData consumed_data(0, false);
  if (span.empty() && !fin) {
    QUIC_BUG << "span.empty() && !fin";
    return consumed_data;
  }

  if (fin_buffered_) {
    QUIC_BUG << "Fin already buffered";
    return consumed_data;
  }

  if (write_side_closed_) {
    QUIC_DLOG(ERROR) << ENDPOINT << "Stream " << id()
                     << "attempting to write when the write side is closed";
    if (type_ == READ_UNIDIRECTIONAL) {
      CloseConnectionWithDetails(
          QUIC_TRY_TO_WRITE_DATA_ON_READ_UNIDIRECTIONAL_STREAM,
          "Try to send data on read unidirectional stream");
    }
    return consumed_data;
  }

  bool had_buffered_data = HasBufferedData();
  if (CanWriteNewData() || span.empty()) {
    consumed_data.fin_consumed = fin;
    if (!span.empty()) {
      // Buffer all data if buffered data size is below limit.
      QuicStreamOffset offset = send_buffer_.stream_offset();
      consumed_data.bytes_consumed =
          span.SaveMemSlicesInSendBuffer(&send_buffer_);
      if (GetQuicReloadableFlag(quic_stream_too_long) &&
          (offset > send_buffer_.stream_offset() ||
           kMaxStreamLength < send_buffer_.stream_offset())) {
        QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_stream_too_long, 3, 5);
        QUIC_BUG << "Write too many data via stream " << id_;
        CloseConnectionWithDetails(
            QUIC_STREAM_LENGTH_OVERFLOW,
            QuicStrCat("Write too many data via stream ", id_));
        return consumed_data;
      }
      OnDataBuffered(offset, consumed_data.bytes_consumed, nullptr);
    }
  }
  fin_buffered_ = consumed_data.fin_consumed;

  if (!had_buffered_data && (HasBufferedData() || fin_buffered_)) {
    // Write data if there is no buffered data before.
    WriteBufferedData();
  }

  return consumed_data;
}

bool QuicStream::HasPendingRetransmission() const {
  return send_buffer_.HasPendingRetransmission() || fin_lost_;
}

bool QuicStream::IsStreamFrameOutstanding(QuicStreamOffset offset,
                                          QuicByteCount data_length,
                                          bool fin) const {
  return send_buffer_.IsStreamDataOutstanding(offset, data_length) ||
         (fin && fin_outstanding_);
}

QuicConsumedData QuicStream::WritevDataInner(size_t write_length,
                                             QuicStreamOffset offset,
                                             bool fin) {
  StreamSendingState state = fin ? FIN : NO_FIN;
  if (fin && add_random_padding_after_fin_) {
    state = FIN_AND_PADDING;
  }
  return session()->WritevData(this, id(), write_length, offset, state);
}

void QuicStream::CloseReadSide() {
  if (read_side_closed_) {
    return;
  }
  QUIC_DVLOG(1) << ENDPOINT << "Done reading from stream " << id();

  read_side_closed_ = true;
  sequencer_.ReleaseBuffer();

  if (write_side_closed_) {
    QUIC_DVLOG(1) << ENDPOINT << "Closing stream " << id();
    session_->CloseStream(id());
  }
}

void QuicStream::CloseWriteSide() {
  if (write_side_closed_) {
    return;
  }
  QUIC_DVLOG(1) << ENDPOINT << "Done writing to stream " << id();

  write_side_closed_ = true;
  if (read_side_closed_) {
    QUIC_DVLOG(1) << ENDPOINT << "Closing stream " << id();
    session_->CloseStream(id());
  }
}

bool QuicStream::HasBufferedData() const {
  DCHECK_GE(send_buffer_.stream_offset(), stream_bytes_written());
  return send_buffer_.stream_offset() > stream_bytes_written();
}

QuicTransportVersion QuicStream::transport_version() const {
  return session_->connection()->transport_version();
}

HandshakeProtocol QuicStream::handshake_protocol() const {
  return session_->connection()->version().handshake_protocol;
}

void QuicStream::StopReading() {
  QUIC_DVLOG(1) << ENDPOINT << "Stop reading from stream " << id();
  sequencer_.StopReading();
}

const QuicSocketAddress& QuicStream::PeerAddressOfLatestPacket() const {
  return session_->connection()->last_packet_source_address();
}

void QuicStream::OnClose() {
  CloseReadSide();
  CloseWriteSide();

  if (!fin_sent_ && !rst_sent_) {
    // For flow control accounting, tell the peer how many bytes have been
    // written on this stream before termination. Done here if needed, using a
    // RST_STREAM frame.
    QUIC_DLOG(INFO) << ENDPOINT << "Sending RST_STREAM in OnClose: " << id();
    session_->SendRstStream(id(), QUIC_RST_ACKNOWLEDGEMENT,
                            stream_bytes_written());
    session_->OnStreamDoneWaitingForAcks(id_);
    rst_sent_ = true;
  }

  if (flow_controller_.FlowControlViolation() ||
      connection_flow_controller_->FlowControlViolation()) {
    return;
  }
  // The stream is being closed and will not process any further incoming bytes.
  // As there may be more bytes in flight, to ensure that both endpoints have
  // the same connection level flow control state, mark all unreceived or
  // buffered bytes as consumed.
  QuicByteCount bytes_to_consume =
      flow_controller_.highest_received_byte_offset() -
      flow_controller_.bytes_consumed();
  AddBytesConsumed(bytes_to_consume);
}

void QuicStream::OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) {
  if (flow_controller_.UpdateSendWindowOffset(frame.byte_offset)) {
    // Let session unblock this stream.
    session_->MarkConnectionLevelWriteBlocked(id_);
  }
}

bool QuicStream::MaybeIncreaseHighestReceivedOffset(
    QuicStreamOffset new_offset) {
  uint64_t increment =
      new_offset - flow_controller_.highest_received_byte_offset();
  if (!flow_controller_.UpdateHighestReceivedOffset(new_offset)) {
    return false;
  }

  // If |new_offset| increased the stream flow controller's highest received
  // offset, increase the connection flow controller's value by the incremental
  // difference.
  if (stream_contributes_to_connection_flow_control_) {
    connection_flow_controller_->UpdateHighestReceivedOffset(
        connection_flow_controller_->highest_received_byte_offset() +
        increment);
  }
  return true;
}

void QuicStream::AddBytesSent(QuicByteCount bytes) {
  flow_controller_.AddBytesSent(bytes);
  if (stream_contributes_to_connection_flow_control_) {
    connection_flow_controller_->AddBytesSent(bytes);
  }
}

void QuicStream::AddBytesConsumed(QuicByteCount bytes) {
  // Only adjust stream level flow controller if still reading.
  if (!read_side_closed_) {
    flow_controller_.AddBytesConsumed(bytes);
  }

  if (stream_contributes_to_connection_flow_control_) {
    connection_flow_controller_->AddBytesConsumed(bytes);
  }
}

void QuicStream::UpdateSendWindowOffset(QuicStreamOffset new_window) {
  if (flow_controller_.UpdateSendWindowOffset(new_window)) {
    // Let session unblock this stream.
    session_->MarkConnectionLevelWriteBlocked(id_);
  }
}

void QuicStream::AddRandomPaddingAfterFin() {
  add_random_padding_after_fin_ = true;
}

bool QuicStream::OnStreamFrameAcked(QuicStreamOffset offset,
                                    QuicByteCount data_length,
                                    bool fin_acked,
                                    QuicTime::Delta ack_delay_time) {
  QUIC_DVLOG(1) << ENDPOINT << "stream " << id_ << " Acking "
                << "[" << offset << ", " << offset + data_length << "]"
                << " fin = " << fin_acked;
  QuicByteCount newly_acked_length = 0;
  if (!send_buffer_.OnStreamDataAcked(offset, data_length,
                                      &newly_acked_length)) {
    CloseConnectionWithDetails(QUIC_INTERNAL_ERROR,
                               "Trying to ack unsent data.");
    return false;
  }
  if (!fin_sent_ && fin_acked) {
    CloseConnectionWithDetails(QUIC_INTERNAL_ERROR,
                               "Trying to ack unsent fin.");
    return false;
  }
  // Indicates whether ack listener's OnPacketAcked should be called.
  const bool new_data_acked =
      newly_acked_length > 0 || (fin_acked && fin_outstanding_);
  if (fin_acked) {
    fin_outstanding_ = false;
    fin_lost_ = false;
  }
  if (!IsWaitingForAcks()) {
    session_->OnStreamDoneWaitingForAcks(id_);
  }
  if (ack_listener_ != nullptr && new_data_acked) {
    ack_listener_->OnPacketAcked(newly_acked_length, ack_delay_time);
  }
  return new_data_acked;
}

void QuicStream::OnStreamFrameRetransmitted(QuicStreamOffset offset,
                                            QuicByteCount data_length,
                                            bool fin_retransmitted) {
  send_buffer_.OnStreamDataRetransmitted(offset, data_length);
  if (fin_retransmitted) {
    fin_lost_ = false;
  }
  if (ack_listener_ != nullptr) {
    ack_listener_->OnPacketRetransmitted(data_length);
  }
}

void QuicStream::OnStreamFrameLost(QuicStreamOffset offset,
                                   QuicByteCount data_length,
                                   bool fin_lost) {
  QUIC_DVLOG(1) << ENDPOINT << "stream " << id_ << " Losting "
                << "[" << offset << ", " << offset + data_length << "]"
                << " fin = " << fin_lost;
  if (data_length > 0) {
    send_buffer_.OnStreamDataLost(offset, data_length);
  }
  if (fin_lost && fin_outstanding_) {
    fin_lost_ = true;
  }
}

bool QuicStream::RetransmitStreamData(QuicStreamOffset offset,
                                      QuicByteCount data_length,
                                      bool fin) {
  if (HasDeadlinePassed()) {
    OnDeadlinePassed();
    return true;
  }
  QuicIntervalSet<QuicStreamOffset> retransmission(offset,
                                                   offset + data_length);
  retransmission.Difference(bytes_acked());
  bool retransmit_fin = fin && fin_outstanding_;
  if (retransmission.Empty() && !retransmit_fin) {
    return true;
  }
  QuicConsumedData consumed(0, false);
  for (const auto& interval : retransmission) {
    QuicStreamOffset retransmission_offset = interval.min();
    QuicByteCount retransmission_length = interval.max() - interval.min();
    const bool can_bundle_fin =
        retransmit_fin && (retransmission_offset + retransmission_length ==
                           stream_bytes_written());
    consumed = session()->WritevData(this, id_, retransmission_length,
                                     retransmission_offset,
                                     can_bundle_fin ? FIN : NO_FIN);
    QUIC_DVLOG(1) << ENDPOINT << "stream " << id_
                  << " is forced to retransmit stream data ["
                  << retransmission_offset << ", "
                  << retransmission_offset + retransmission_length
                  << ") and fin: " << can_bundle_fin
                  << ", consumed: " << consumed;
    OnStreamFrameRetransmitted(retransmission_offset, consumed.bytes_consumed,
                               consumed.fin_consumed);
    if (can_bundle_fin) {
      retransmit_fin = !consumed.fin_consumed;
    }
    if (consumed.bytes_consumed < retransmission_length ||
        (can_bundle_fin && !consumed.fin_consumed)) {
      // Connection is write blocked.
      return false;
    }
  }
  if (retransmit_fin) {
    QUIC_DVLOG(1) << ENDPOINT << "stream " << id_
                  << " retransmits fin only frame.";
    consumed = session()->WritevData(this, id_, 0, stream_bytes_written(), FIN);
    if (!consumed.fin_consumed) {
      return false;
    }
  }
  return true;
}

bool QuicStream::IsWaitingForAcks() const {
  return (!rst_sent_ || stream_error_ == QUIC_STREAM_NO_ERROR) &&
         (send_buffer_.stream_bytes_outstanding() || fin_outstanding_);
}

size_t QuicStream::ReadableBytes() const {
  return sequencer_.ReadableBytes();
}

bool QuicStream::WriteStreamData(QuicStreamOffset offset,
                                 QuicByteCount data_length,
                                 QuicDataWriter* writer) {
  DCHECK_LT(0u, data_length);
  QUIC_DVLOG(2) << ENDPOINT << "Write stream " << id_ << " data from offset "
                << offset << " length " << data_length;
  return send_buffer_.WriteStreamData(offset, data_length, writer);
}

void QuicStream::WriteBufferedData() {
  DCHECK(!write_side_closed_ && (HasBufferedData() || fin_buffered_));

  if (session_->ShouldYield(id())) {
    session_->MarkConnectionLevelWriteBlocked(id());
    return;
  }

  // Size of buffered data.
  size_t write_length = BufferedDataBytes();

  // A FIN with zero data payload should not be flow control blocked.
  bool fin_with_zero_data = (fin_buffered_ && write_length == 0);

  bool fin = fin_buffered_;

  // How much data flow control permits to be written.
  QuicByteCount send_window = flow_controller_.SendWindowSize();
  if (stream_contributes_to_connection_flow_control_) {
    send_window =
        std::min(send_window, connection_flow_controller_->SendWindowSize());
  }

  if (send_window == 0 && !fin_with_zero_data) {
    // Quick return if nothing can be sent.
    MaybeSendBlocked();
    return;
  }

  if (write_length > send_window) {
    // Don't send the FIN unless all the data will be sent.
    fin = false;

    // Writing more data would be a violation of flow control.
    write_length = static_cast<size_t>(send_window);
    QUIC_DVLOG(1) << "stream " << id() << " shortens write length to "
                  << write_length << " due to flow control";
  }
  if (session_->session_decides_what_to_write()) {
    session_->SetTransmissionType(NOT_RETRANSMISSION);
  }
  QuicConsumedData consumed_data =
      WritevDataInner(write_length, stream_bytes_written(), fin);

  OnStreamDataConsumed(consumed_data.bytes_consumed);

  AddBytesSent(consumed_data.bytes_consumed);
  QUIC_DVLOG(1) << ENDPOINT << "stream " << id_ << " sends "
                << stream_bytes_written() << " bytes "
                << " and has buffered data " << BufferedDataBytes() << " bytes."
                << " fin is sent: " << consumed_data.fin_consumed
                << " fin is buffered: " << fin_buffered_;

  // The write may have generated a write error causing this stream to be
  // closed. If so, simply return without marking the stream write blocked.
  if (write_side_closed_) {
    return;
  }

  if (consumed_data.bytes_consumed == write_length) {
    if (!fin_with_zero_data) {
      MaybeSendBlocked();
    }
    if (fin && consumed_data.fin_consumed) {
      fin_sent_ = true;
      fin_outstanding_ = true;
      if (fin_received_) {
        session_->StreamDraining(id_);
      }
      CloseWriteSide();
    } else if (fin && !consumed_data.fin_consumed) {
      session_->MarkConnectionLevelWriteBlocked(id());
    }
  } else {
    session_->MarkConnectionLevelWriteBlocked(id());
  }
  if (consumed_data.bytes_consumed > 0 || consumed_data.fin_consumed) {
    busy_counter_ = 0;
  }
}

uint64_t QuicStream::BufferedDataBytes() const {
  DCHECK_GE(send_buffer_.stream_offset(), stream_bytes_written());
  return send_buffer_.stream_offset() - stream_bytes_written();
}

bool QuicStream::CanWriteNewData() const {
  return BufferedDataBytes() < buffered_data_threshold_;
}

uint64_t QuicStream::stream_bytes_written() const {
  return send_buffer_.stream_bytes_written();
}

const QuicIntervalSet<QuicStreamOffset>& QuicStream::bytes_acked() const {
  return send_buffer_.bytes_acked();
}

void QuicStream::OnStreamDataConsumed(size_t bytes_consumed) {
  send_buffer_.OnStreamDataConsumed(bytes_consumed);
}

void QuicStream::WritePendingRetransmission() {
  while (HasPendingRetransmission()) {
    QuicConsumedData consumed(0, false);
    if (!send_buffer_.HasPendingRetransmission()) {
      QUIC_DVLOG(1) << ENDPOINT << "stream " << id_
                    << " retransmits fin only frame.";
      consumed =
          session()->WritevData(this, id_, 0, stream_bytes_written(), FIN);
      fin_lost_ = !consumed.fin_consumed;
      if (fin_lost_) {
        // Connection is write blocked.
        return;
      }
    } else {
      StreamPendingRetransmission pending =
          send_buffer_.NextPendingRetransmission();
      // Determine whether the lost fin can be bundled with the data.
      const bool can_bundle_fin =
          fin_lost_ &&
          (pending.offset + pending.length == stream_bytes_written());
      consumed =
          session()->WritevData(this, id_, pending.length, pending.offset,
                                can_bundle_fin ? FIN : NO_FIN);
      QUIC_DVLOG(1) << ENDPOINT << "stream " << id_
                    << " tries to retransmit stream data [" << pending.offset
                    << ", " << pending.offset + pending.length
                    << ") and fin: " << can_bundle_fin
                    << ", consumed: " << consumed;
      OnStreamFrameRetransmitted(pending.offset, consumed.bytes_consumed,
                                 consumed.fin_consumed);
      if (consumed.bytes_consumed < pending.length ||
          (can_bundle_fin && !consumed.fin_consumed)) {
        // Connection is write blocked.
        return;
      }
    }
  }
}

bool QuicStream::MaybeSetTtl(QuicTime::Delta ttl) {
  if (is_static_) {
    QUIC_BUG << "Cannot set TTL of a static stream.";
    return false;
  }
  if (deadline_.IsInitialized()) {
    QUIC_DLOG(WARNING) << "Deadline has already been set.";
    return false;
  }
  if (!session()->session_decides_what_to_write()) {
    QUIC_DLOG(WARNING) << "This session does not support stream TTL yet.";
    return false;
  }
  QuicTime now = session()->connection()->clock()->ApproximateNow();
  deadline_ = now + ttl;
  return true;
}

bool QuicStream::HasDeadlinePassed() const {
  if (!deadline_.IsInitialized()) {
    // No deadline has been set.
    return false;
  }
  DCHECK(session()->session_decides_what_to_write());
  QuicTime now = session()->connection()->clock()->ApproximateNow();
  if (now < deadline_) {
    return false;
  }
  // TTL expired.
  QUIC_DVLOG(1) << "stream " << id() << " deadline has passed";
  return true;
}

void QuicStream::OnDeadlinePassed() {
  Reset(QUIC_STREAM_TTL_EXPIRED);
}

}  // namespace quic
