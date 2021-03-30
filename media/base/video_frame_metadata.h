// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/video_transformation.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

struct MEDIA_EXPORT VideoFrameMetadata {
  VideoFrameMetadata();
  ~VideoFrameMetadata() = default;

  VideoFrameMetadata(const VideoFrameMetadata& other);

  enum CopyMode {
    // Indicates that mailbox created in one context, is also being used in a
    // different context belonging to another share group and video frames are
    // using SurfaceTexture to render frames.
    // Textures generated from SurfaceTexture can't be shared between contexts
    // of different share group and hence this frame must be copied to a new
    // texture before use, rather than being used directly.
    kCopyToNewTexture = 0,

    // Indicates that mailbox created in one context, is also being used in a
    // different context belonging to another share group and video frames are
    // using AImageReader to render frames.
    // AImageReader allows to render image data to AHardwareBuffer which can be
    // shared between contexts of different share group. AHB from existing
    // mailbox is wrapped into a new mailbox(AHB backed) which can then be used
    // by another context.
    kCopyMailboxesOnly = 1,
  };

  // Merges internal values from |metadata_source|.
  void MergeMetadataFrom(const VideoFrameMetadata& metadata_source);

  // Sources of VideoFrames use this marker to indicate that the associated
  // VideoFrame can be overlaid, case in which its contents do not need to be
  // further composited but displayed directly.
  bool allow_overlay = false;

  // Video capture begin/end timestamps.  Consumers can use these values for
  // dynamic optimizations, logging stats, etc.
  base::Optional<base::TimeTicks> capture_begin_time;
  base::Optional<base::TimeTicks> capture_end_time;

  // A counter that is increased by the producer of video frames each time
  // it pushes out a new frame. By looking for gaps in this counter, clients
  // can determine whether or not any frames have been dropped on the way from
  // the producer between two consecutively received frames. Note that the
  // counter may start at arbitrary values, so the absolute value of it has no
  // meaning.
  base::Optional<int> capture_counter;

  // The rectangular region of the frame that has changed since the frame
  // with the directly preceding CAPTURE_COUNTER. If that frame was not
  // received, typically because it was dropped during transport from the
  // producer, clients must assume that the entire frame has changed.
  // The rectangle is relative to the full frame data, i.e. [0, 0,
  // coded_size().width(), coded_size().height()]. It does not have to be
  // fully contained within visible_rect().
  base::Optional<gfx::Rect> capture_update_rect;

  // If not null, it indicates how video frame mailbox should be copied to a
  // new mailbox.
  base::Optional<CopyMode> copy_mode;

  // Indicates if the current frame is the End of its current Stream.
  bool end_of_stream = false;

  // The estimated duration of this frame (i.e., the amount of time between
  // the media timestamp of this frame and the next).  Note that this is not
  // the same information provided by FRAME_RATE as the FRAME_DURATION can
  // vary unpredictably for every frame.  Consumers can use this to optimize
  // playback scheduling, make encoding quality decisions, and/or compute
  // frame-level resource utilization stats.
  base::Optional<base::TimeDelta> frame_duration;

  // Represents either the fixed frame rate, or the maximum frame rate to
  // expect from a variable-rate source.  This value generally remains the
  // same for all frames in the same session.
  base::Optional<double> frame_rate;

  // This is a boolean that signals that the video capture engine detects
  // interactive content. One possible optimization that this signal can help
  // with is remote content: adjusting end-to-end latency down to help the
  // user better coordinate their actions.
  bool interactive_content = false;

  // This field represents the local time at which either: 1) the frame was
  // generated, if it was done so locally; or 2) the targeted play-out time
  // of the frame, if it was generated from a remote source. This value is NOT
  // a high-resolution timestamp, and so it should not be used as a
  // presentation time; but, instead, it should be used for buffering playback
  // and for A/V synchronization purposes.
  base::Optional<base::TimeTicks> reference_time;

  // Sources of VideoFrames use this marker to indicate that an instance of
  // VideoFrameExternalResources produced from the associated video frame
  // should use read lock fences.
  bool read_lock_fences_enabled = false;

  // Indicates that the frame has a rotation and/or flip.
  base::Optional<VideoTransformation> transformation;

  // Android only: if set, then this frame is not suitable for overlay, even
  // if ALLOW_OVERLAY is set.  However, it allows us to process the overlay
  // to see if it would have been promoted, if it were backed by a SurfaceView
  // instead.  This lets us figure out when SurfaceViews are appropriate.
  bool texture_owner = false;

  // Android only: if set, then this frame's resource would like to be
  // notified about its promotability to an overlay.
  bool wants_promotion_hint = false;

  // This video frame comes from protected content.
  bool protected_video = false;

  // This video frame is protected by hardware. This option is valid only if
  // PROTECTED_VIDEO is also set to true.
  bool hw_protected = false;

  // Identifier used to query if a HW protected video frame can still be
  // properly displayed or not. Non-zero when valid.
  uint32_t hw_protected_validation_id = 0;

  // An UnguessableToken that identifies VideoOverlayFactory that created
  // this VideoFrame. It's used by Cast to help with video hole punch.
  base::Optional<base::UnguessableToken> overlay_plane_id;

  // Whether this frame was decoded in a power efficient way.
  bool power_efficient = false;

  // CompositorFrameMetadata variables associated with this frame. Used for
  // remote debugging.
  // TODO(crbug.com/832220): Use a customized dictionary value instead of
  // using these keys directly.
  base::Optional<double> device_scale_factor;
  base::Optional<double> page_scale_factor;
  base::Optional<double> root_scroll_offset_x;
  base::Optional<double> root_scroll_offset_y;
  base::Optional<double> top_controls_visible_height;

  // If present, this field represents the local time at which the VideoFrame
  // was decoded from whichever format it was encoded in. Sometimes only
  // DECODE_END_TIME will be present.
  base::Optional<base::TimeTicks> decode_begin_time;
  base::Optional<base::TimeTicks> decode_end_time;

  // If present, this field represents the elapsed time from the submission of
  // the encoded packet with the same PTS as this frame to the decoder until
  // the decoded frame was ready for presentation.
  base::Optional<base::TimeDelta> processing_time;

  // The RTP timestamp associated with this video frame. Stored as a double
  // since base::DictionaryValue doesn't have a uint32_t type.
  //
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource-rtptimestamp
  base::Optional<double> rtp_timestamp;

  // For video frames coming from a remote source, this is the time the
  // encoded frame was received by the platform, i.e., the time at
  // which the last packet belonging to this frame was received over the
  // network.
  base::Optional<base::TimeTicks> receive_time;

  // If present, this field represents the duration this frame is ideally
  // expected to spend on the screen during playback. Unlike FRAME_DURATION
  // this field takes into account current playback rate.
  base::Optional<base::TimeDelta> wallclock_frame_duration;

  // WebRTC streams only: if present, this field represents the maximum
  // composition delay that is allowed for this frame. This is respected
  // in a best effort manner.
  // This is an experimental feature, see crbug.com/1138888 for more
  // information.
  base::Optional<int> maximum_composition_delay_in_frames;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
