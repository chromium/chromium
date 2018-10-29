// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_frame.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_logging.h"

using std::string;

namespace quic {

QuicFrame::QuicFrame() {}

QuicFrame::QuicFrame(QuicPaddingFrame padding_frame)
    : padding_frame(padding_frame) {}

QuicFrame::QuicFrame(QuicStreamFrame stream_frame)
    : stream_frame(stream_frame) {}

QuicFrame::QuicFrame(QuicCryptoFrame* crypto_frame)
    : type(CRYPTO_FRAME), crypto_frame(crypto_frame) {}

QuicFrame::QuicFrame(QuicAckFrame* frame) : type(ACK_FRAME), ack_frame(frame) {}

QuicFrame::QuicFrame(QuicMtuDiscoveryFrame frame)
    : mtu_discovery_frame(frame) {}

QuicFrame::QuicFrame(QuicStopWaitingFrame* frame)
    : type(STOP_WAITING_FRAME), stop_waiting_frame(frame) {}

QuicFrame::QuicFrame(QuicPingFrame frame) : ping_frame(frame) {}

QuicFrame::QuicFrame(QuicRstStreamFrame* frame)
    : type(RST_STREAM_FRAME), rst_stream_frame(frame) {}

QuicFrame::QuicFrame(QuicConnectionCloseFrame* frame)
    : type(CONNECTION_CLOSE_FRAME), connection_close_frame(frame) {}

QuicFrame::QuicFrame(QuicGoAwayFrame* frame)
    : type(GOAWAY_FRAME), goaway_frame(frame) {}

QuicFrame::QuicFrame(QuicWindowUpdateFrame* frame)
    : type(WINDOW_UPDATE_FRAME), window_update_frame(frame) {}

QuicFrame::QuicFrame(QuicBlockedFrame* frame)
    : type(BLOCKED_FRAME), blocked_frame(frame) {}

QuicFrame::QuicFrame(QuicApplicationCloseFrame* frame)
    : type(APPLICATION_CLOSE_FRAME), application_close_frame(frame) {}

QuicFrame::QuicFrame(QuicNewConnectionIdFrame* frame)
    : type(NEW_CONNECTION_ID_FRAME), new_connection_id_frame(frame) {}

QuicFrame::QuicFrame(QuicRetireConnectionIdFrame* frame)
    : type(RETIRE_CONNECTION_ID_FRAME), retire_connection_id_frame(frame) {}

QuicFrame::QuicFrame(QuicMaxStreamIdFrame frame) : max_stream_id_frame(frame) {}

QuicFrame::QuicFrame(QuicStreamIdBlockedFrame frame)
    : stream_id_blocked_frame(frame) {}

QuicFrame::QuicFrame(QuicPathResponseFrame* frame)
    : type(PATH_RESPONSE_FRAME), path_response_frame(frame) {}

QuicFrame::QuicFrame(QuicPathChallengeFrame* frame)
    : type(PATH_CHALLENGE_FRAME), path_challenge_frame(frame) {}

QuicFrame::QuicFrame(QuicStopSendingFrame* frame)
    : type(STOP_SENDING_FRAME), stop_sending_frame(frame) {}

QuicFrame::QuicFrame(QuicMessageFrame* frame)
    : type(MESSAGE_FRAME), message_frame(frame) {}

QuicFrame::QuicFrame(QuicNewTokenFrame* frame)
    : type(NEW_TOKEN_FRAME), new_token_frame(frame) {}

void DeleteFrames(QuicFrames* frames) {
  for (QuicFrame& frame : *frames) {
    DeleteFrame(&frame);
  }
  frames->clear();
}

void DeleteFrame(QuicFrame* frame) {
  switch (frame->type) {
    // Frames smaller than a pointer are inlined, so don't need to be deleted.
    case PADDING_FRAME:
    case MTU_DISCOVERY_FRAME:
    case PING_FRAME:
    case MAX_STREAM_ID_FRAME:
    case STREAM_ID_BLOCKED_FRAME:
    case STREAM_FRAME:
      break;
    case ACK_FRAME:
      delete frame->ack_frame;
      break;
    case STOP_WAITING_FRAME:
      delete frame->stop_waiting_frame;
      break;
    case RST_STREAM_FRAME:
      delete frame->rst_stream_frame;
      break;
    case CONNECTION_CLOSE_FRAME:
      delete frame->connection_close_frame;
      break;
    case GOAWAY_FRAME:
      delete frame->goaway_frame;
      break;
    case BLOCKED_FRAME:
      delete frame->blocked_frame;
      break;
    case WINDOW_UPDATE_FRAME:
      delete frame->window_update_frame;
      break;
    case PATH_CHALLENGE_FRAME:
      delete frame->path_challenge_frame;
      break;
    case STOP_SENDING_FRAME:
      delete frame->stop_sending_frame;
      break;
    case APPLICATION_CLOSE_FRAME:
      delete frame->application_close_frame;
      break;
    case NEW_CONNECTION_ID_FRAME:
      delete frame->new_connection_id_frame;
      break;
    case RETIRE_CONNECTION_ID_FRAME:
      delete frame->retire_connection_id_frame;
      break;
    case PATH_RESPONSE_FRAME:
      delete frame->path_response_frame;
      break;
    case MESSAGE_FRAME:
      delete frame->message_frame;
      break;
    case CRYPTO_FRAME:
      delete frame->crypto_frame;
      break;
    case NEW_TOKEN_FRAME:
      delete frame->new_token_frame;
      break;

    case NUM_FRAME_TYPES:
      DCHECK(false) << "Cannot delete type: " << frame->type;
  }
}

void RemoveFramesForStream(QuicFrames* frames, QuicStreamId stream_id) {
  auto it = frames->begin();
  while (it != frames->end()) {
    if (it->type != STREAM_FRAME || it->stream_frame.stream_id != stream_id) {
      ++it;
      continue;
    }
    it = frames->erase(it);
  }
}

bool IsControlFrame(QuicFrameType type) {
  switch (type) {
    case RST_STREAM_FRAME:
    case GOAWAY_FRAME:
    case WINDOW_UPDATE_FRAME:
    case BLOCKED_FRAME:
    case STREAM_ID_BLOCKED_FRAME:
    case MAX_STREAM_ID_FRAME:
    case PING_FRAME:
      return true;
    default:
      return false;
  }
}

QuicControlFrameId GetControlFrameId(const QuicFrame& frame) {
  switch (frame.type) {
    case RST_STREAM_FRAME:
      return frame.rst_stream_frame->control_frame_id;
    case GOAWAY_FRAME:
      return frame.goaway_frame->control_frame_id;
    case WINDOW_UPDATE_FRAME:
      return frame.window_update_frame->control_frame_id;
    case BLOCKED_FRAME:
      return frame.blocked_frame->control_frame_id;
    case STREAM_ID_BLOCKED_FRAME:
      return frame.stream_id_blocked_frame.control_frame_id;
    case MAX_STREAM_ID_FRAME:
      return frame.max_stream_id_frame.control_frame_id;
    case PING_FRAME:
      return frame.ping_frame.control_frame_id;
    default:
      return kInvalidControlFrameId;
  }
}

void SetControlFrameId(QuicControlFrameId control_frame_id, QuicFrame* frame) {
  switch (frame->type) {
    case RST_STREAM_FRAME:
      frame->rst_stream_frame->control_frame_id = control_frame_id;
      return;
    case GOAWAY_FRAME:
      frame->goaway_frame->control_frame_id = control_frame_id;
      return;
    case WINDOW_UPDATE_FRAME:
      frame->window_update_frame->control_frame_id = control_frame_id;
      return;
    case BLOCKED_FRAME:
      frame->blocked_frame->control_frame_id = control_frame_id;
      return;
    case PING_FRAME:
      frame->ping_frame.control_frame_id = control_frame_id;
      return;
    case STREAM_ID_BLOCKED_FRAME:
      frame->stream_id_blocked_frame.control_frame_id = control_frame_id;
      return;
    case MAX_STREAM_ID_FRAME:
      frame->max_stream_id_frame.control_frame_id = control_frame_id;
      return;
    default:
      QUIC_BUG
          << "Try to set control frame id of a frame without control frame id";
  }
}

QuicFrame CopyRetransmittableControlFrame(const QuicFrame& frame) {
  QuicFrame copy;
  switch (frame.type) {
    case RST_STREAM_FRAME:
      copy = QuicFrame(new QuicRstStreamFrame(*frame.rst_stream_frame));
      break;
    case GOAWAY_FRAME:
      copy = QuicFrame(new QuicGoAwayFrame(*frame.goaway_frame));
      break;
    case WINDOW_UPDATE_FRAME:
      copy = QuicFrame(new QuicWindowUpdateFrame(*frame.window_update_frame));
      break;
    case BLOCKED_FRAME:
      copy = QuicFrame(new QuicBlockedFrame(*frame.blocked_frame));
      break;
    case PING_FRAME:
      copy = QuicFrame(QuicPingFrame(frame.ping_frame.control_frame_id));
      break;
    default:
      QUIC_BUG << "Try to copy a non-retransmittable control frame: " << frame;
      copy = QuicFrame(QuicPingFrame(kInvalidControlFrameId));
      break;
  }
  return copy;
}

std::ostream& operator<<(std::ostream& os, const QuicFrame& frame) {
  switch (frame.type) {
    case PADDING_FRAME: {
      os << "type { PADDING_FRAME } " << frame.padding_frame;
      break;
    }
    case RST_STREAM_FRAME: {
      os << "type { RST_STREAM_FRAME } " << *(frame.rst_stream_frame);
      break;
    }
    case CONNECTION_CLOSE_FRAME: {
      os << "type { CONNECTION_CLOSE_FRAME } "
         << *(frame.connection_close_frame);
      break;
    }
    case GOAWAY_FRAME: {
      os << "type { GOAWAY_FRAME } " << *(frame.goaway_frame);
      break;
    }
    case WINDOW_UPDATE_FRAME: {
      os << "type { WINDOW_UPDATE_FRAME } " << *(frame.window_update_frame);
      break;
    }
    case BLOCKED_FRAME: {
      os << "type { BLOCKED_FRAME } " << *(frame.blocked_frame);
      break;
    }
    case STREAM_FRAME: {
      os << "type { STREAM_FRAME } " << frame.stream_frame;
      break;
    }
    case ACK_FRAME: {
      os << "type { ACK_FRAME } " << *(frame.ack_frame);
      break;
    }
    case STOP_WAITING_FRAME: {
      os << "type { STOP_WAITING_FRAME } " << *(frame.stop_waiting_frame);
      break;
    }
    case PING_FRAME: {
      os << "type { PING_FRAME } " << frame.ping_frame;
      break;
    }
    case MTU_DISCOVERY_FRAME: {
      os << "type { MTU_DISCOVERY_FRAME } ";
      break;
    }
    case APPLICATION_CLOSE_FRAME:
      os << "type { APPLICATION_CLOSE } " << *(frame.connection_close_frame);
      break;
    case NEW_CONNECTION_ID_FRAME:
      os << "type { NEW_CONNECTION_ID } " << *(frame.new_connection_id_frame);
      break;
    case RETIRE_CONNECTION_ID_FRAME:
      os << "type { RETIRE_CONNECTION_ID } "
         << *(frame.retire_connection_id_frame);
      break;
    case MAX_STREAM_ID_FRAME:
      os << "type { MAX_STREAM_ID } " << frame.max_stream_id_frame;
      break;
    case STREAM_ID_BLOCKED_FRAME:
      os << "type { STREAM_ID_BLOCKED } " << frame.stream_id_blocked_frame;
      break;
    case PATH_RESPONSE_FRAME:
      os << "type { PATH_RESPONSE } " << *(frame.path_response_frame);
      break;
    case PATH_CHALLENGE_FRAME:
      os << "type { PATH_CHALLENGE } " << *(frame.path_challenge_frame);
      break;
    case STOP_SENDING_FRAME:
      os << "type { STOP_SENDING } " << *(frame.stop_sending_frame);
      break;
    case MESSAGE_FRAME:
      os << "type { MESSAGE_FRAME }" << *(frame.message_frame);
      break;
    case NEW_TOKEN_FRAME:
      os << "type { NEW_TOKEN_FRAME }" << *(frame.new_token_frame);
      break;
    default: {
      QUIC_LOG(ERROR) << "Unknown frame type: " << frame.type;
      break;
    }
  }
  return os;
}

}  // namespace quic
