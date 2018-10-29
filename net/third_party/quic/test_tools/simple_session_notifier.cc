// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simple_session_notifier.h"

#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {

namespace test {

SimpleSessionNotifier::SimpleSessionNotifier(QuicConnection* connection)
    : last_control_frame_id_(kInvalidControlFrameId),
      least_unacked_(1),
      least_unsent_(1),
      connection_(connection) {}

SimpleSessionNotifier::~SimpleSessionNotifier() {
  while (!control_frames_.empty()) {
    DeleteFrame(&control_frames_.front());
    control_frames_.pop_front();
  }
}

SimpleSessionNotifier::StreamState::StreamState()
    : bytes_total(0),
      bytes_sent(0),
      fin_buffered(false),
      fin_sent(false),
      fin_outstanding(false),
      fin_lost(false) {}

SimpleSessionNotifier::StreamState::~StreamState() {}

QuicConsumedData SimpleSessionNotifier::WriteOrBufferData(
    QuicStreamId id,
    QuicByteCount data_length,
    StreamSendingState state) {
  if (!QuicContainsKey(stream_map_, id)) {
    stream_map_[id] = StreamState();
  }
  StreamState& stream_state = stream_map_.find(id)->second;
  const bool had_buffered_data =
      HasBufferedStreamData() || HasBufferedControlFrames();
  QuicConsumedData total_consumed(0, false);
  QuicStreamOffset offset = stream_state.bytes_sent;
  QUIC_DVLOG(1) << "WriteOrBuffer stream_id: " << id << " [" << offset << ", "
                << offset + data_length << "), fin: " << (state != NO_FIN);
  stream_state.bytes_total += data_length;
  stream_state.fin_buffered = state != NO_FIN;
  if (had_buffered_data) {
    QUIC_DLOG(WARNING) << "Connection is write blocked";
    return {0, false};
  }
  const size_t length = stream_state.bytes_total - stream_state.bytes_sent;
  connection_->SetTransmissionType(NOT_RETRANSMISSION);
  QuicConsumedData consumed =
      connection_->SendStreamData(id, length, stream_state.bytes_sent,
                                  stream_state.fin_buffered ? FIN : NO_FIN);
  QUIC_DVLOG(1) << "consumed: " << consumed;
  OnStreamDataConsumed(id, stream_state.bytes_sent, consumed.bytes_consumed,
                       consumed.fin_consumed);
  return consumed;
}

void SimpleSessionNotifier::OnStreamDataConsumed(QuicStreamId id,
                                                 QuicStreamOffset offset,
                                                 QuicByteCount data_length,
                                                 bool fin) {
  StreamState& state = stream_map_.find(id)->second;
  if (id == QuicUtils::GetCryptoStreamId(connection_->transport_version()) &&
      data_length > 0) {
    crypto_bytes_transferred_[connection_->encryption_level()].Add(
        offset, offset + data_length);
  }
  state.bytes_sent += data_length;
  state.fin_sent = fin;
  state.fin_outstanding = fin;
}

void SimpleSessionNotifier::WriteOrBufferRstStream(
    QuicStreamId id,
    QuicRstStreamErrorCode error,
    QuicStreamOffset bytes_written) {
  QUIC_DVLOG(1) << "Writing RST_STREAM_FRAME";
  const bool had_buffered_data =
      HasBufferedStreamData() || HasBufferedControlFrames();
  control_frames_.emplace_back((QuicFrame(new QuicRstStreamFrame(
      ++last_control_frame_id_, id, error, bytes_written))));
  if (error != QUIC_STREAM_NO_ERROR) {
    // Delete stream to avoid retransmissions.
    stream_map_.erase(id);
  }
  if (had_buffered_data) {
    QUIC_DLOG(WARNING) << "Connection is write blocked";
    return;
  }
  WriteBufferedControlFrames();
}

void SimpleSessionNotifier::NeuterUnencryptedData() {
  for (const auto& interval : crypto_bytes_transferred_[ENCRYPTION_NONE]) {
    QuicStreamFrame stream_frame(
        QuicUtils::GetCryptoStreamId(connection_->transport_version()), false,
        interval.min(), interval.max() - interval.min());
    OnFrameAcked(QuicFrame(stream_frame), QuicTime::Delta::Zero());
  }
}

void SimpleSessionNotifier::OnCanWrite() {
  if (!RetransmitLostCryptoData() || !RetransmitLostControlFrames() ||
      !RetransmitLostStreamData()) {
    return;
  }
  // Write buffered control frames.
  if (!WriteBufferedControlFrames()) {
    return;
  }
  // Write new data.
  for (const auto& pair : stream_map_) {
    const auto& state = pair.second;
    if (!StreamHasBufferedData(pair.first)) {
      continue;
    }

    const size_t length = state.bytes_total - state.bytes_sent;
    const bool can_bundle_fin =
        state.fin_buffered && (state.bytes_sent + length == state.bytes_total);
    connection_->SetTransmissionType(NOT_RETRANSMISSION);
    QuicConsumedData consumed = connection_->SendStreamData(
        pair.first, length, state.bytes_sent, can_bundle_fin ? FIN : NO_FIN);
    QUIC_DVLOG(1) << "Tries to write stream_id: " << pair.first << " ["
                  << state.bytes_sent << ", " << state.bytes_sent + length
                  << "), fin: " << can_bundle_fin
                  << ", and consumed: " << consumed;
    OnStreamDataConsumed(pair.first, state.bytes_sent, consumed.bytes_consumed,
                         consumed.fin_consumed);
    if (length != consumed.bytes_consumed ||
        (can_bundle_fin && !consumed.fin_consumed)) {
      break;
    }
  }
}

bool SimpleSessionNotifier::WillingToWrite() const {
  QUIC_DVLOG(1) << "has_buffered_control_frames: " << HasBufferedControlFrames()
                << " as_lost_control_frames: " << !lost_control_frames_.empty()
                << " has_buffered_stream_data: " << HasBufferedStreamData()
                << " has_lost_stream_data: " << HasLostStreamData();
  return HasBufferedControlFrames() || !lost_control_frames_.empty() ||
         HasBufferedStreamData() || HasLostStreamData();
}

QuicByteCount SimpleSessionNotifier::StreamBytesSent() const {
  QuicByteCount bytes_sent = 0;
  for (const auto& pair : stream_map_) {
    const auto& state = pair.second;
    bytes_sent += state.bytes_sent;
  }
  return bytes_sent;
}

QuicByteCount SimpleSessionNotifier::StreamBytesToSend() const {
  QuicByteCount bytes_to_send = 0;
  for (const auto& pair : stream_map_) {
    const auto& state = pair.second;
    bytes_to_send += (state.bytes_total - state.bytes_sent);
  }
  return bytes_to_send;
}

bool SimpleSessionNotifier::OnFrameAcked(const QuicFrame& frame,
                                         QuicTime::Delta /*ack_delay_time*/) {
  QUIC_DVLOG(1) << "Acking " << frame;
  if (frame.type != STREAM_FRAME) {
    return OnControlFrameAcked(frame);
  }
  if (!QuicContainsKey(stream_map_, frame.stream_frame.stream_id)) {
    return false;
  }
  auto* state = &stream_map_.find(frame.stream_frame.stream_id)->second;
  QuicStreamOffset offset = frame.stream_frame.offset;
  QuicByteCount data_length = frame.stream_frame.data_length;
  QuicIntervalSet<QuicStreamOffset> newly_acked(offset, offset + data_length);
  newly_acked.Difference(state->bytes_acked);
  const bool fin_newly_acked = frame.stream_frame.fin && state->fin_outstanding;
  if (newly_acked.Empty() && !fin_newly_acked) {
    return false;
  }
  state->bytes_acked.Add(offset, offset + data_length);
  if (fin_newly_acked) {
    state->fin_outstanding = false;
    state->fin_lost = false;
  }
  state->pending_retransmissions.Difference(offset, offset + data_length);
  return true;
}

void SimpleSessionNotifier::OnFrameLost(const QuicFrame& frame) {
  QUIC_DVLOG(1) << "Losting " << frame;
  if (frame.type != STREAM_FRAME) {
    OnControlFrameLost(frame);
    return;
  }
  if (!QuicContainsKey(stream_map_, frame.stream_frame.stream_id)) {
    return;
  }
  auto* state = &stream_map_.find(frame.stream_frame.stream_id)->second;
  QuicStreamOffset offset = frame.stream_frame.offset;
  QuicByteCount data_length = frame.stream_frame.data_length;
  QuicIntervalSet<QuicStreamOffset> bytes_lost(offset, offset + data_length);
  bytes_lost.Difference(state->bytes_acked);
  const bool fin_lost = state->fin_outstanding && frame.stream_frame.fin;
  if (bytes_lost.Empty() && !fin_lost) {
    return;
  }
  for (const auto& lost : bytes_lost) {
    state->pending_retransmissions.Add(lost.min(), lost.max());
  }
  state->fin_lost = fin_lost;
}

void SimpleSessionNotifier::RetransmitFrames(const QuicFrames& frames,
                                             TransmissionType type) {
  QuicConnection::ScopedPacketFlusher retransmission_flusher(
      connection_, QuicConnection::SEND_ACK_IF_QUEUED);
  connection_->SetTransmissionType(type);
  for (const QuicFrame& frame : frames) {
    if (frame.type != STREAM_FRAME) {
      if (GetControlFrameId(frame) == kInvalidControlFrameId) {
        continue;
      }
      QuicFrame copy = CopyRetransmittableControlFrame(frame);
      if (!connection_->SendControlFrame(copy)) {
        // Connection is write blocked.
        DeleteFrame(&copy);
        return;
      }
      continue;
    }
    if (!QuicContainsKey(stream_map_, frame.stream_frame.stream_id)) {
      continue;
    }
    const auto& state = stream_map_.find(frame.stream_frame.stream_id)->second;
    QuicIntervalSet<QuicStreamOffset> retransmission(
        frame.stream_frame.offset,
        frame.stream_frame.offset + frame.stream_frame.data_length);
    EncryptionLevel retransmission_encryption_level =
        connection_->encryption_level();
    EncryptionLevel current_encryption_level = connection_->encryption_level();
    if (frame.stream_frame.stream_id ==
        QuicUtils::GetCryptoStreamId(connection_->transport_version())) {
      for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
        if (retransmission.Intersects(crypto_bytes_transferred_[i])) {
          retransmission_encryption_level = static_cast<EncryptionLevel>(i);
          retransmission.Intersection(crypto_bytes_transferred_[i]);
          break;
        }
      }
    }
    retransmission.Difference(state.bytes_acked);
    bool retransmit_fin = frame.stream_frame.fin && state.fin_outstanding;
    QuicConsumedData consumed(0, false);
    for (const auto& interval : retransmission) {
      QuicStreamOffset retransmission_offset = interval.min();
      QuicByteCount retransmission_length = interval.max() - interval.min();
      const bool can_bundle_fin =
          retransmit_fin &&
          (retransmission_offset + retransmission_length == state.bytes_sent);
      if (frame.stream_frame.stream_id ==
          QuicUtils::GetCryptoStreamId(connection_->transport_version())) {
        // Set appropriate encryption level for crypto stream.
        connection_->SetDefaultEncryptionLevel(retransmission_encryption_level);
      }
      consumed = connection_->SendStreamData(
          frame.stream_frame.stream_id, retransmission_length,
          retransmission_offset, can_bundle_fin ? FIN : NO_FIN);
      QUIC_DVLOG(1) << "stream " << frame.stream_frame.stream_id
                    << " is forced to retransmit stream data ["
                    << retransmission_offset << ", "
                    << retransmission_offset + retransmission_length
                    << ") and fin: " << can_bundle_fin
                    << ", consumed: " << consumed;
      if (can_bundle_fin) {
        retransmit_fin = !consumed.fin_consumed;
      }
      if (frame.stream_frame.stream_id ==
          QuicUtils::GetCryptoStreamId(connection_->transport_version())) {
        // Restore encryption level.
        connection_->SetDefaultEncryptionLevel(current_encryption_level);
      }
      if (consumed.bytes_consumed < retransmission_length ||
          (can_bundle_fin && !consumed.fin_consumed)) {
        // Connection is write blocked.
        return;
      }
    }
    if (retransmit_fin) {
      QUIC_DVLOG(1) << "stream " << frame.stream_frame.stream_id
                    << " retransmits fin only frame.";
      consumed = connection_->SendStreamData(frame.stream_frame.stream_id, 0,
                                             state.bytes_sent, FIN);
    }
  }
}

bool SimpleSessionNotifier::IsFrameOutstanding(const QuicFrame& frame) const {
  if (frame.type != STREAM_FRAME) {
    return IsControlFrameOutstanding(frame);
  }
  if (!QuicContainsKey(stream_map_, frame.stream_frame.stream_id)) {
    return false;
  }
  const auto& state = stream_map_.find(frame.stream_frame.stream_id)->second;
  QuicStreamOffset offset = frame.stream_frame.offset;
  QuicByteCount data_length = frame.stream_frame.data_length;
  return (data_length > 0 &&
          !state.bytes_acked.Contains(offset, offset + data_length)) ||
         (frame.stream_frame.fin && state.fin_outstanding);
}

bool SimpleSessionNotifier::HasUnackedCryptoData() const {
  if (!QuicContainsKey(stream_map_, QuicUtils::GetCryptoStreamId(
                                        connection_->transport_version()))) {
    return false;
  }
  const auto& state =
      stream_map_
          .find(QuicUtils::GetCryptoStreamId(connection_->transport_version()))
          ->second;
  if (state.bytes_total > state.bytes_sent) {
    return true;
  }
  QuicIntervalSet<QuicStreamOffset> bytes_to_ack(0, state.bytes_total);
  bytes_to_ack.Difference(state.bytes_acked);
  return !bytes_to_ack.Empty();
}

bool SimpleSessionNotifier::OnControlFrameAcked(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    return false;
  }
  DCHECK(id < least_unacked_ + control_frames_.size());
  if (id < least_unacked_ ||
      GetControlFrameId(control_frames_.at(id - least_unacked_)) ==
          kInvalidControlFrameId) {
    return false;
  }
  SetControlFrameId(kInvalidControlFrameId,
                    &control_frames_.at(id - least_unacked_));
  lost_control_frames_.erase(id);
  while (!control_frames_.empty() &&
         GetControlFrameId(control_frames_.front()) == kInvalidControlFrameId) {
    DeleteFrame(&control_frames_.front());
    control_frames_.pop_front();
    ++least_unacked_;
  }
  return true;
}

void SimpleSessionNotifier::OnControlFrameLost(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    return;
  }
  DCHECK(id < least_unacked_ + control_frames_.size());
  if (id < least_unacked_ ||
      GetControlFrameId(control_frames_.at(id - least_unacked_)) ==
          kInvalidControlFrameId) {
    return;
  }
  if (!QuicContainsKey(lost_control_frames_, id)) {
    lost_control_frames_[id] = true;
  }
}

bool SimpleSessionNotifier::IsControlFrameOutstanding(
    const QuicFrame& frame) const {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    return false;
  }
  return id < least_unacked_ + control_frames_.size() && id >= least_unacked_ &&
         GetControlFrameId(control_frames_.at(id - least_unacked_)) !=
             kInvalidControlFrameId;
}

bool SimpleSessionNotifier::RetransmitLostControlFrames() {
  while (!lost_control_frames_.empty()) {
    QuicFrame pending = control_frames_.at(lost_control_frames_.begin()->first -
                                           least_unacked_);
    QuicFrame copy = CopyRetransmittableControlFrame(pending);
    connection_->SetTransmissionType(LOSS_RETRANSMISSION);
    if (!connection_->SendControlFrame(copy)) {
      // Connection is write blocked.
      DeleteFrame(&copy);
      break;
    }
    lost_control_frames_.pop_front();
  }
  return lost_control_frames_.empty();
}

bool SimpleSessionNotifier::RetransmitLostCryptoData() {
  if (!QuicContainsKey(stream_map_, QuicUtils::GetCryptoStreamId(
                                        connection_->transport_version()))) {
    return true;
  }
  auto& state =
      stream_map_
          .find(QuicUtils::GetCryptoStreamId(connection_->transport_version()))
          ->second;
  while (!state.pending_retransmissions.Empty()) {
    connection_->SetTransmissionType(HANDSHAKE_RETRANSMISSION);
    QuicIntervalSet<QuicStreamOffset> retransmission(
        state.pending_retransmissions.begin()->min(),
        state.pending_retransmissions.begin()->max());
    EncryptionLevel retransmission_encryption_level = ENCRYPTION_NONE;
    for (size_t i = 0; i < NUM_ENCRYPTION_LEVELS; ++i) {
      if (retransmission.Intersects(crypto_bytes_transferred_[i])) {
        retransmission_encryption_level = static_cast<EncryptionLevel>(i);
        retransmission.Intersection(crypto_bytes_transferred_[i]);
        break;
      }
    }
    QuicStreamOffset retransmission_offset = retransmission.begin()->min();
    QuicByteCount retransmission_length =
        retransmission.begin()->max() - retransmission.begin()->min();
    EncryptionLevel current_encryption_level = connection_->encryption_level();
    // Set appropriate encryption level.
    connection_->SetDefaultEncryptionLevel(retransmission_encryption_level);
    QuicConsumedData consumed = connection_->SendStreamData(
        QuicUtils::GetCryptoStreamId(connection_->transport_version()),
        retransmission_length, retransmission_offset, NO_FIN);
    // Restore encryption level.
    connection_->SetDefaultEncryptionLevel(current_encryption_level);
    state.pending_retransmissions.Difference(
        retransmission_offset, retransmission_offset + consumed.bytes_consumed);
    if (consumed.bytes_consumed < retransmission_length) {
      break;
    }
  }
  return state.pending_retransmissions.Empty();
}

bool SimpleSessionNotifier::RetransmitLostStreamData() {
  for (auto& pair : stream_map_) {
    StreamState& state = pair.second;
    QuicConsumedData consumed(0, false);
    while (!state.pending_retransmissions.Empty() || state.fin_lost) {
      connection_->SetTransmissionType(LOSS_RETRANSMISSION);
      if (state.pending_retransmissions.Empty()) {
        QUIC_DVLOG(1) << "stream " << pair.first
                      << " retransmits fin only frame.";
        consumed =
            connection_->SendStreamData(pair.first, 0, state.bytes_sent, FIN);
        state.fin_lost = !consumed.fin_consumed;
        if (state.fin_lost) {
          DLOG(INFO) << "Connection is write blocked";
          return false;
        }
      } else {
        QuicStreamOffset offset = state.pending_retransmissions.begin()->min();
        QuicByteCount length = state.pending_retransmissions.begin()->max() -
                               state.pending_retransmissions.begin()->min();
        const bool can_bundle_fin =
            state.fin_lost && (offset + length == state.bytes_sent);
        consumed = connection_->SendStreamData(pair.first, length, offset,
                                               can_bundle_fin ? FIN : NO_FIN);
        QUIC_DVLOG(1) << "stream " << pair.first
                      << " tries to retransmit stream data [" << offset << ", "
                      << offset + length << ") and fin: " << can_bundle_fin
                      << ", consumed: " << consumed;
        state.pending_retransmissions.Difference(
            offset, offset + consumed.bytes_consumed);
        if (consumed.fin_consumed) {
          state.fin_lost = false;
        }
        if (length > consumed.bytes_consumed ||
            (can_bundle_fin && !consumed.fin_consumed)) {
          DVLOG(1) << "Connection is write blocked";
          break;
        }
      }
    }
  }
  return !HasLostStreamData();
}

bool SimpleSessionNotifier::WriteBufferedControlFrames() {
  while (HasBufferedControlFrames()) {
    QuicFrame frame_to_send =
        control_frames_.at(least_unsent_ - least_unacked_);
    QuicFrame copy = CopyRetransmittableControlFrame(frame_to_send);
    connection_->SetTransmissionType(NOT_RETRANSMISSION);
    if (!connection_->SendControlFrame(copy)) {
      // Connection is write blocked.
      DeleteFrame(&copy);
      break;
    }
    ++least_unsent_;
  }
  return !HasBufferedControlFrames();
}

bool SimpleSessionNotifier::HasBufferedControlFrames() const {
  return least_unsent_ < least_unacked_ + control_frames_.size();
}

bool SimpleSessionNotifier::HasBufferedStreamData() const {
  for (const auto& pair : stream_map_) {
    const auto& state = pair.second;
    if (state.bytes_total > state.bytes_sent ||
        (state.fin_buffered && !state.fin_sent)) {
      return true;
    }
  }
  return false;
}

bool SimpleSessionNotifier::StreamIsWaitingForAcks(QuicStreamId id) const {
  if (!QuicContainsKey(stream_map_, id)) {
    return false;
  }
  const StreamState& state = stream_map_.find(id)->second;
  return !state.bytes_acked.Contains(0, state.bytes_sent) ||
         state.fin_outstanding;
}

bool SimpleSessionNotifier::StreamHasBufferedData(QuicStreamId id) const {
  if (!QuicContainsKey(stream_map_, id)) {
    return false;
  }
  const StreamState& state = stream_map_.find(id)->second;
  return state.bytes_total > state.bytes_sent ||
         (state.fin_buffered && !state.fin_sent);
}

bool SimpleSessionNotifier::HasLostStreamData() const {
  for (const auto& pair : stream_map_) {
    const auto& state = pair.second;
    if (!state.pending_retransmissions.Empty() || state.fin_lost) {
      return true;
    }
  }
  return false;
}

}  // namespace test

}  // namespace quic
