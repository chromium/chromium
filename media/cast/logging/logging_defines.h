// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
#define MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/common/rtp_time.h"

namespace media {
namespace cast {

enum CastLoggingEvent {
  UNKNOWN,
  // Sender side frame events.
  FRAME_CAPTURE_BEGIN,
  FRAME_CAPTURE_END,
  FRAME_ENCODED,
  FRAME_ACK_RECEIVED,
  // Receiver side frame events.
  FRAME_ACK_SENT,
  FRAME_DECODED,
  FRAME_PLAYOUT,
  // Sender side packet events.
  PACKET_SENT_TO_NETWORK,
  PACKET_RETRANSMITTED,
  PACKET_RTX_REJECTED,
  // Receiver side packet events.
  PACKET_RECEIVED,
};
enum {
  kNumOfLoggingEvents = PACKET_RECEIVED + 1,
};

const char* CastLoggingToString(CastLoggingEvent event);

// CastLoggingEvent are classified into one of three following types.
enum EventMediaType {
  AUDIO_EVENT,
  VIDEO_EVENT,
  UNKNOWN_EVENT,
  EVENT_MEDIA_TYPE_LAST = UNKNOWN_EVENT
};

struct FrameEvent {
  FrameEvent();
  FrameEvent(const FrameEvent& other);
  ~FrameEvent();

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id;

  // Resolution of the frame. Only set for video FRAME_CAPTURE_END events.
  int width;
  int height;

  // Size of encoded frame in bytes. Only set for FRAME_ENCODED event.
  // Note: we use uint32_t instead of size_t for byte count because this struct
  // is sent over IPC which could span 32 & 64 bit processes.
  uint32_t size;

  // Time of event logged.
  base::TimeTicks timestamp;

  CastLoggingEvent type;

  EventMediaType media_type;

  // Only set for FRAME_PLAYOUT events.
  // If this value is zero the frame is rendered on time.
  // If this value is positive it means the frame is rendered late.
  // If this value is negative it means the frame is rendered early.
  base::TimeDelta delay_delta;

  // Whether the frame is a key frame. Only set for video FRAME_ENCODED event.
  bool key_frame;

  // The requested target bitrate of the encoder at the time the frame is
  // encoded. Only set for video FRAME_ENCODED event.
  int target_bitrate;

  // Encoding performance metrics. See media/cast/common/sender_encoded_frame.h
  // for a description of these values.
  double encoder_cpu_utilization;
  double idealized_bitrate_utilization;
};

struct PacketEvent {
  PacketEvent();
  ~PacketEvent();

  RtpTimeTicks rtp_timestamp;
  FrameId frame_id;
  uint16_t max_packet_id;
  uint16_t packet_id;
  uint32_t size;

  // Time of event logged.
  base::TimeTicks timestamp;
  CastLoggingEvent type;
  EventMediaType media_type;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_DEFINES_H_
