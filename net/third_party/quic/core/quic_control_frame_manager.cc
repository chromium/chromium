// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_control_frame_manager.h"

#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

QuicControlFrameManager::QuicControlFrameManager(QuicSession* session)
    : last_control_frame_id_(kInvalidControlFrameId),
      least_unacked_(1),
      least_unsent_(1),
      session_(session),
      donot_retransmit_old_window_updates_(
          GetQuicReloadableFlag(quic_donot_retransmit_old_window_update2)) {}

QuicControlFrameManager::~QuicControlFrameManager() {
  while (!control_frames_.empty()) {
    DeleteFrame(&control_frames_.front());
    control_frames_.pop_front();
  }
}

void QuicControlFrameManager::WriteOrBufferRstStream(
    QuicStreamId id,
    QuicRstStreamErrorCode error,
    QuicStreamOffset bytes_written) {
  QUIC_DVLOG(1) << "Writing RST_STREAM_FRAME";
  const bool had_buffered_frames = HasBufferedFrames();
  control_frames_.emplace_back((QuicFrame(new QuicRstStreamFrame(
      ++last_control_frame_id_, id, error, bytes_written))));
  if (had_buffered_frames) {
    return;
  }
  WriteBufferedFrames();
}

void QuicControlFrameManager::WriteOrBufferGoAway(
    QuicErrorCode error,
    QuicStreamId last_good_stream_id,
    const QuicString& reason) {
  QUIC_DVLOG(1) << "Writing GOAWAY_FRAME";
  const bool had_buffered_frames = HasBufferedFrames();
  control_frames_.emplace_back(QuicFrame(new QuicGoAwayFrame(
      ++last_control_frame_id_, error, last_good_stream_id, reason)));
  if (had_buffered_frames) {
    return;
  }
  WriteBufferedFrames();
}

void QuicControlFrameManager::WriteOrBufferWindowUpdate(
    QuicStreamId id,
    QuicStreamOffset byte_offset) {
  QUIC_DVLOG(1) << "Writing WINDOW_UPDATE_FRAME";
  const bool had_buffered_frames = HasBufferedFrames();
  control_frames_.emplace_back(QuicFrame(
      new QuicWindowUpdateFrame(++last_control_frame_id_, id, byte_offset)));
  if (had_buffered_frames) {
    return;
  }
  WriteBufferedFrames();
}

void QuicControlFrameManager::WriteOrBufferBlocked(QuicStreamId id) {
  QUIC_DVLOG(1) << "Writing BLOCKED_FRAME";
  const bool had_buffered_frames = HasBufferedFrames();
  control_frames_.emplace_back(
      QuicFrame(new QuicBlockedFrame(++last_control_frame_id_, id)));
  if (had_buffered_frames) {
    return;
  }
  WriteBufferedFrames();
}

void QuicControlFrameManager::WritePing() {
  QUIC_DVLOG(1) << "Writing PING_FRAME";
  if (HasBufferedFrames()) {
    // Do not send ping if there is buffered frames.
    QUIC_LOG(WARNING)
        << "Try to send PING when there is buffered control frames.";
    return;
  }
  control_frames_.emplace_back(
      QuicFrame(QuicPingFrame(++last_control_frame_id_)));
  WriteBufferedFrames();
}

void QuicControlFrameManager::OnControlFrameSent(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    QUIC_BUG
        << "Send or retransmit a control frame with invalid control frame id";
    return;
  }
  if (donot_retransmit_old_window_updates_ &&
      frame.type == WINDOW_UPDATE_FRAME) {
    QuicStreamId stream_id = frame.window_update_frame->stream_id;
    if (QuicContainsKey(window_update_frames_, stream_id) &&
        id > window_update_frames_[stream_id]) {
      // Consider the older window update of the same stream as acked.
      QUIC_FLAG_COUNT(
          quic_reloadable_flag_quic_donot_retransmit_old_window_update2);
      OnControlFrameIdAcked(window_update_frames_[stream_id]);
    }
    window_update_frames_[stream_id] = id;
  }
  if (QuicContainsKey(pending_retransmissions_, id)) {
    // This is retransmitted control frame.
    pending_retransmissions_.erase(id);
    return;
  }
  if (id > least_unsent_) {
    QUIC_BUG << "Try to send control frames out of order, id: " << id
             << " least_unsent: " << least_unsent_;
    session_->connection()->CloseConnection(
        QUIC_INTERNAL_ERROR, "Try to send control frames out of order",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  ++least_unsent_;
}

bool QuicControlFrameManager::OnControlFrameAcked(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (!OnControlFrameIdAcked(id)) {
    return false;
  }
  if (donot_retransmit_old_window_updates_ &&
      frame.type == WINDOW_UPDATE_FRAME) {
    QuicStreamId stream_id = frame.window_update_frame->stream_id;
    if (QuicContainsKey(window_update_frames_, stream_id) &&
        window_update_frames_[stream_id] == id) {
      window_update_frames_.erase(stream_id);
    }
  }
  return true;
}

void QuicControlFrameManager::OnControlFrameLost(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    // Frame does not have a valid control frame ID, ignore it.
    return;
  }
  if (id >= least_unsent_) {
    QUIC_BUG << "Try to mark unsent control frame as lost";
    session_->connection()->CloseConnection(
        QUIC_INTERNAL_ERROR, "Try to mark unsent control frame as lost",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  if (id < least_unacked_ ||
      GetControlFrameId(control_frames_.at(id - least_unacked_)) ==
          kInvalidControlFrameId) {
    // This frame has already been acked.
    return;
  }
  if (!QuicContainsKey(pending_retransmissions_, id)) {
    pending_retransmissions_[id] = true;
  }
}

bool QuicControlFrameManager::IsControlFrameOutstanding(
    const QuicFrame& frame) const {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    // Frame without a control frame ID should not be retransmitted.
    return false;
  }
  // Consider this frame is outstanding if it does not get acked.
  return id < least_unacked_ + control_frames_.size() && id >= least_unacked_ &&
         GetControlFrameId(control_frames_.at(id - least_unacked_)) !=
             kInvalidControlFrameId;
}

bool QuicControlFrameManager::HasPendingRetransmission() const {
  return !pending_retransmissions_.empty();
}

bool QuicControlFrameManager::WillingToWrite() const {
  return HasPendingRetransmission() || HasBufferedFrames();
}

QuicFrame QuicControlFrameManager::NextPendingRetransmission() const {
  QUIC_BUG_IF(pending_retransmissions_.empty())
      << "Unexpected call to NextPendingRetransmission() with empty pending "
      << "retransmission list.";
  QuicControlFrameId id = pending_retransmissions_.begin()->first;
  return control_frames_.at(id - least_unacked_);
}

void QuicControlFrameManager::OnCanWrite() {
  if (HasPendingRetransmission()) {
    // Exit early to allow streams to write pending retransmissions if any.
    WritePendingRetransmission();
    return;
  }
  WriteBufferedFrames();
}

bool QuicControlFrameManager::RetransmitControlFrame(const QuicFrame& frame) {
  QuicControlFrameId id = GetControlFrameId(frame);
  if (id == kInvalidControlFrameId) {
    // Frame does not have a valid control frame ID, ignore it. Returns true
    // to allow writing following frames.
    return true;
  }
  if (id >= least_unsent_) {
    QUIC_BUG << "Try to retransmit unsent control frame";
    session_->connection()->CloseConnection(
        QUIC_INTERNAL_ERROR, "Try to retransmit unsent control frame",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return false;
  }
  if (id < least_unacked_ ||
      GetControlFrameId(control_frames_.at(id - least_unacked_)) ==
          kInvalidControlFrameId) {
    // This frame has already been acked.
    return true;
  }
  QuicFrame copy = CopyRetransmittableControlFrame(frame);
  QUIC_DVLOG(1) << "control frame manager is forced to retransmit frame: "
                << frame;
  if (session_->WriteControlFrame(copy)) {
    return true;
  }
  DeleteFrame(&copy);
  return false;
}

void QuicControlFrameManager::WriteBufferedFrames() {
  while (HasBufferedFrames()) {
    if (session_->session_decides_what_to_write()) {
      session_->SetTransmissionType(NOT_RETRANSMISSION);
    }
    QuicFrame frame_to_send =
        control_frames_.at(least_unsent_ - least_unacked_);
    QuicFrame copy = CopyRetransmittableControlFrame(frame_to_send);
    if (!session_->WriteControlFrame(copy)) {
      // Connection is write blocked.
      DeleteFrame(&copy);
      break;
    }
    OnControlFrameSent(frame_to_send);
  }
}

void QuicControlFrameManager::WritePendingRetransmission() {
  while (HasPendingRetransmission()) {
    QuicFrame pending = NextPendingRetransmission();
    QuicFrame copy = CopyRetransmittableControlFrame(pending);
    if (!session_->WriteControlFrame(copy)) {
      // Connection is write blocked.
      DeleteFrame(&copy);
      break;
    }
    OnControlFrameSent(pending);
  }
}

bool QuicControlFrameManager::OnControlFrameIdAcked(QuicControlFrameId id) {
  if (id == kInvalidControlFrameId) {
    // Frame does not have a valid control frame ID, ignore it.
    return false;
  }
  if (id >= least_unsent_) {
    QUIC_BUG << "Try to ack unsent control frame";
    session_->connection()->CloseConnection(
        QUIC_INTERNAL_ERROR, "Try to ack unsent control frame",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return false;
  }
  if (id < least_unacked_ ||
      GetControlFrameId(control_frames_.at(id - least_unacked_)) ==
          kInvalidControlFrameId) {
    // This frame has already been acked.
    return false;
  }

  // Set control frame ID of acked frames to 0.
  SetControlFrameId(kInvalidControlFrameId,
                    &control_frames_.at(id - least_unacked_));
  // Remove acked control frames from pending retransmissions.
  pending_retransmissions_.erase(id);
  // Clean up control frames queue and increment least_unacked_.
  while (!control_frames_.empty() &&
         GetControlFrameId(control_frames_.front()) == kInvalidControlFrameId) {
    DeleteFrame(&control_frames_.front());
    control_frames_.pop_front();
    ++least_unacked_;
  }
  return true;
}

bool QuicControlFrameManager::HasBufferedFrames() const {
  return least_unsent_ < least_unacked_ + control_frames_.size();
}

}  // namespace quic
