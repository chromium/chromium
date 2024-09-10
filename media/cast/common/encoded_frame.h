// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_ENCODED_FRAME_H_
#define MEDIA_CAST_COMMON_ENCODED_FRAME_H_

#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/frame_id.h"
#include "media/cast/common/rtp_time.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

namespace media {
namespace cast {

// A combination of metadata and data for one encoded frame.  This can contain
// audio data or video data or other.
struct EncodedFrame {
  EncodedFrame();
  virtual ~EncodedFrame();

  // Convenience accessors to data as an array of uint8_t elements.
  base::span<const uint8_t> bytes() const { return base::as_byte_span(data); }
  base::span<uint8_t> mutable_bytes() {
    return base::as_writable_byte_span(data);
  }

  // Copies all data members except |data| to |dest|.
  // Does not modify |dest->data|.
  void CopyMetadataTo(EncodedFrame* dest) const;

  // This frame's dependency relationship with respect to other frames.
  openscreen::cast::EncodedFrame::Dependency dependency =
      openscreen::cast::EncodedFrame::Dependency::kUnknown;

  // The label associated with this frame.  Implies an ordering relative to
  // other frames in the same stream.
  FrameId frame_id;

  // The label associated with the frame upon which this frame depends.  If
  // this frame does not require any other frame in order to become decodable
  // (e.g., key frames), |referenced_frame_id| must equal |frame_id|.
  FrameId referenced_frame_id;

  // The stream timestamp, on the timeline of the signal data.  For example, RTP
  // timestamps for audio are usually defined as the total number of audio
  // samples encoded in all prior frames.  A playback system uses this value to
  // detect gaps in the stream, and otherwise stretch the signal to match
  // playout targets.
  RtpTimeTicks rtp_timestamp;

  // The common reference clock timestamp for this frame.  This value originates
  // from a sender and is used to provide lip synchronization between streams in
  // a receiver.  Thus, in the sender context, this is set to the time at which
  // the frame was captured/recorded.  In the receiver context, this is set to
  // the target playout time.  Over a sequence of frames, this time value is
  // expected to drift with respect to the elapsed time implied by the RTP
  // timestamps; and it may not necessarily increment with precise regularity.
  base::TimeTicks reference_time;

  // Playout delay for this and all future frames. Used by the Adaptive
  // Playout delay extension. Zero means no change.
  uint16_t new_playout_delay_ms = 0;

  // The encoded signal data.
  std::string data;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_ENCODED_FRAME_H_
