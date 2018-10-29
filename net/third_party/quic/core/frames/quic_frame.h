// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_FRAME_H_

#include <ostream>
#include <vector>

#include "net/third_party/quic/core/frames/quic_ack_frame.h"
#include "net/third_party/quic/core/frames/quic_application_close_frame.h"
#include "net/third_party/quic/core/frames/quic_blocked_frame.h"
#include "net/third_party/quic/core/frames/quic_connection_close_frame.h"
#include "net/third_party/quic/core/frames/quic_crypto_frame.h"
#include "net/third_party/quic/core/frames/quic_goaway_frame.h"
#include "net/third_party/quic/core/frames/quic_max_stream_id_frame.h"
#include "net/third_party/quic/core/frames/quic_message_frame.h"
#include "net/third_party/quic/core/frames/quic_mtu_discovery_frame.h"
#include "net/third_party/quic/core/frames/quic_new_connection_id_frame.h"
#include "net/third_party/quic/core/frames/quic_new_token_frame.h"
#include "net/third_party/quic/core/frames/quic_padding_frame.h"
#include "net/third_party/quic/core/frames/quic_path_challenge_frame.h"
#include "net/third_party/quic/core/frames/quic_path_response_frame.h"
#include "net/third_party/quic/core/frames/quic_ping_frame.h"
#include "net/third_party/quic/core/frames/quic_retire_connection_id_frame.h"
#include "net/third_party/quic/core/frames/quic_rst_stream_frame.h"
#include "net/third_party/quic/core/frames/quic_stop_sending_frame.h"
#include "net/third_party/quic/core/frames/quic_stop_waiting_frame.h"
#include "net/third_party/quic/core/frames/quic_stream_frame.h"
#include "net/third_party/quic/core/frames/quic_stream_id_blocked_frame.h"
#include "net/third_party/quic/core/frames/quic_window_update_frame.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicFrame {
  QuicFrame();
  // Please keep the constructors in the same order as the union below.
  explicit QuicFrame(QuicPaddingFrame padding_frame);
  explicit QuicFrame(QuicMtuDiscoveryFrame frame);
  explicit QuicFrame(QuicPingFrame frame);
  explicit QuicFrame(QuicMaxStreamIdFrame frame);
  explicit QuicFrame(QuicStreamIdBlockedFrame frame);
  explicit QuicFrame(QuicStreamFrame stream_frame);

  explicit QuicFrame(QuicAckFrame* frame);
  explicit QuicFrame(QuicRstStreamFrame* frame);
  explicit QuicFrame(QuicConnectionCloseFrame* frame);
  explicit QuicFrame(QuicStopWaitingFrame* frame);
  explicit QuicFrame(QuicGoAwayFrame* frame);
  explicit QuicFrame(QuicWindowUpdateFrame* frame);
  explicit QuicFrame(QuicBlockedFrame* frame);
  explicit QuicFrame(QuicApplicationCloseFrame* frame);
  explicit QuicFrame(QuicNewConnectionIdFrame* frame);
  explicit QuicFrame(QuicRetireConnectionIdFrame* frame);
  explicit QuicFrame(QuicNewTokenFrame* frame);
  explicit QuicFrame(QuicPathResponseFrame* frame);
  explicit QuicFrame(QuicPathChallengeFrame* frame);
  explicit QuicFrame(QuicStopSendingFrame* frame);
  explicit QuicFrame(QuicMessageFrame* message_frame);
  explicit QuicFrame(QuicCryptoFrame* crypto_frame);

  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& os,
                                                      const QuicFrame& frame);

  union {
    // Inlined frames.
    // Overlapping inlined frames have a |type| field at the same 0 offset as
    // QuicFrame does for out of line frames below, allowing use of the
    // remaining 7 bytes after offset for frame-type specific fields.
    QuicPaddingFrame padding_frame;
    QuicMtuDiscoveryFrame mtu_discovery_frame;
    QuicPingFrame ping_frame;
    QuicMaxStreamIdFrame max_stream_id_frame;
    QuicStreamIdBlockedFrame stream_id_blocked_frame;
    QuicStreamFrame stream_frame;

    // Out of line frames.
    struct {
      QuicFrameType type;

      // TODO(wub): These frames can also be inlined without increasing the size
      // of QuicFrame: QuicStopWaitingFrame, QuicRstStreamFrame,
      // QuicWindowUpdateFrame, QuicBlockedFrame, QuicPathResponseFrame,
      // QuicPathChallengeFrame and QuicStopSendingFrame.
      union {
        QuicAckFrame* ack_frame;
        QuicStopWaitingFrame* stop_waiting_frame;
        QuicRstStreamFrame* rst_stream_frame;
        QuicConnectionCloseFrame* connection_close_frame;
        QuicGoAwayFrame* goaway_frame;
        QuicWindowUpdateFrame* window_update_frame;
        QuicBlockedFrame* blocked_frame;
        QuicApplicationCloseFrame* application_close_frame;
        QuicNewConnectionIdFrame* new_connection_id_frame;
        QuicRetireConnectionIdFrame* retire_connection_id_frame;
        QuicPathResponseFrame* path_response_frame;
        QuicPathChallengeFrame* path_challenge_frame;
        QuicStopSendingFrame* stop_sending_frame;
        QuicMessageFrame* message_frame;
        QuicCryptoFrame* crypto_frame;
        QuicNewTokenFrame* new_token_frame;
      };
    };
  };
};

static_assert(sizeof(QuicFrame) <= 24,
              "Frames larger than 24 bytes should be referenced by pointer.");
static_assert(offsetof(QuicStreamFrame, type) == offsetof(QuicFrame, type),
              "Offset of |type| must match in QuicFrame and QuicStreamFrame");

// A inline size of 1 is chosen to optimize the typical use case of
// 1-stream-frame in QuicTransmissionInfo.retransmittable_frames.
typedef QuicInlinedVector<QuicFrame, 1> QuicFrames;

// Deletes all the sub-frames contained in |frames|.
QUIC_EXPORT_PRIVATE void DeleteFrames(QuicFrames* frames);

// Delete the sub-frame contained in |frame|.
QUIC_EXPORT_PRIVATE void DeleteFrame(QuicFrame* frame);

// Deletes all the QuicStreamFrames for the specified |stream_id|.
QUIC_EXPORT_PRIVATE void RemoveFramesForStream(QuicFrames* frames,
                                               QuicStreamId stream_id);

// Returns true if |type| is a retransmittable control frame.
QUIC_EXPORT_PRIVATE bool IsControlFrame(QuicFrameType type);

// Returns control_frame_id of |frame|. Returns kInvalidControlFrameId if
// |frame| does not have a valid control_frame_id.
QUIC_EXPORT_PRIVATE QuicControlFrameId
GetControlFrameId(const QuicFrame& frame);

// Sets control_frame_id of |frame| to |control_frame_id|.
QUIC_EXPORT_PRIVATE void SetControlFrameId(QuicControlFrameId control_frame_id,
                                           QuicFrame* frame);

// Returns a copy of |frame|.
QUIC_EXPORT_PRIVATE QuicFrame
CopyRetransmittableControlFrame(const QuicFrame& frame);

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_FRAME_H_
