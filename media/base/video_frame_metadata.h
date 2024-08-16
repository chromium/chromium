// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include <optional>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/video_transformation.h"
#include "media/gpu/buildflags.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// NOTE: When adding new VideoFrameMetadata fields, please ensure you update the
// MergeMetadataFrom() method.
struct MEDIA_EXPORT VideoFrameMetadata {
  VideoFrameMetadata();
  ~VideoFrameMetadata() = default;

  VideoFrameMetadata(const VideoFrameMetadata& other);

  // Merges internal values from |metadata_source|.
  void MergeMetadataFrom(const VideoFrameMetadata& metadata_source);

  // Clear metadata fields that only make sense for texture backed frames.
  void ClearTextureFrameMetadata();

  // Sources of VideoFrames use this marker to indicate that the associated
  // VideoFrame can be overlaid, case in which its contents do not need to be
  // further composited but displayed directly.
  bool allow_overlay = false;

  // Video capture begin/end timestamps.  Consumers can use these values for
  // dynamic optimizations, logging stats, etc.
  std::optional<base::TimeTicks> capture_begin_time;
  std::optional<base::TimeTicks> capture_end_time;

  // A counter that is increased by the producer of video frames each time
  // it pushes out a new frame. By looking for gaps in this counter, clients
  // can determine whether or not any frames have been dropped on the way from
  // the producer between two consecutively received frames. Note that the
  // counter may start at arbitrary values, so the absolute value of it has no
  // meaning.
  std::optional<int> capture_counter;

  // The rectangular region of the frame that has changed since the frame
  // with the directly preceding CAPTURE_COUNTER. If that frame was not
  // received, typically because it was dropped during transport from the
  // producer, clients must assume that the entire frame has changed.
  // The rectangle is relative to the full frame data, i.e. [0, 0,
  // coded_size().width(), coded_size().height()]. It does not have to be
  // fully contained within visible_rect().
  std::optional<gfx::Rect> capture_update_rect;

  // For encoded frames, this is the original source size which may be different
  // from the encoded size. It's used for the HiDPI tab capture heuristic.
  // The size corresponds to the active region if region capture is active,
  // or otherwise the full size of the captured source.
  std::optional<gfx::Size> source_size;

  // If cropping was applied due to Region Capture to produce this frame,
  // then this reflects where the frame's contents originate from in the
  // original uncropped frame.
  //
  // NOTE: May also be nullopt if region capture is enabled but the capture rect
  // is in a different coordinate space. For more info, see
  // https://crbug.com/1327560.
  std::optional<gfx::Rect> region_capture_rect;

  // Whenever cropTo() or restrictTo() are called, Blink increments the
  // sub_capture_target_version and records a Promise as associated with that
  // sub_capture_target_version. When Blink observes a frame with this new
  // version or a later one, Blink resolves the Promise. Frames associated with
  // a source which cannot be cropped will always have this value set to zero.
  uint32_t sub_capture_target_version = 0;

  // Indicates that mailbox created in one context, is also being used in a
  // different context belonging to another share group and video frames are
  // using SurfaceTexture to render frames.
  // Textures generated from SurfaceTexture can't be shared between contexts
  // of different share group and hence this frame must be copied to a new
  // texture before use, rather than being used directly.
  bool copy_required = false;

  // Indicates if the current frame is the End of its current Stream.
  bool end_of_stream = false;

  // The estimated duration of this frame (i.e., the amount of time between
  // the media timestamp of this frame and the next).  Note that this is not
  // the same information provided by FRAME_RATE as the FRAME_DURATION can
  // vary unpredictably for every frame.  Consumers can use this to optimize
  // playback scheduling, make encoding quality decisions, and/or compute
  // frame-level resource utilization stats.
  std::optional<base::TimeDelta> frame_duration;

  // Represents either the fixed frame rate, or the maximum frame rate to
  // expect from a variable-rate source.  This value generally remains the
  // same for all frames in the same session.
  std::optional<double> frame_rate;

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
  std::optional<base::TimeTicks> reference_time;

  // Sources of VideoFrames use this marker to indicate that an instance of
  // VideoFrameExternalResources produced from the associated video frame
  // should use read lock fences.
  bool read_lock_fences_enabled = false;

  // Indicates that the frame has a rotation and/or flip.
  std::optional<VideoTransformation> transformation;

  // Android only: if set, then this frame is not suitable for overlay, even
  // if ALLOW_OVERLAY is set.  However, it allows us to process the overlay
  // to see if it would have been promoted, if it were backed by a SurfaceView
  // instead.  This lets us figure out when SurfaceViews are appropriate.
  bool texture_owner = false;

  // Android & Windows only: if set, then this frame's resource would like to
  // be notified about its promotability to an overlay.
  bool wants_promotion_hint = false;

  // Windows only: set when frame is backed by a dcomp surface handle.
  bool dcomp_surface = false;

  // This video frame comes from protected content.
  bool protected_video = false;

  // This video frame is protected by hardware. This option is valid only if
  // PROTECTED_VIDEO is also set to true.
  bool hw_protected = false;

  // If true, we need to run a detiling image processor on the video before we
  // can scan it out.
  bool needs_detiling = false;

  // This video frame's shared image backing can support zero-copy WebGPU
  // import.
  bool is_webgpu_compatible = false;

#if BUILDFLAG(USE_VAAPI)
  // The ID of the VA-API protected session used to decode this frame, if
  // applicable. The proper type is VAProtectedSessionID. However, in order to
  // avoid including the VA-API headers in this file, we use the underlying
  // type. Users of this field are expected to have compile-time assertions to
  // ensure it's safe to use this as a VAProtectedSessionID.
  //
  // Notes on IPC: this field should not be copied to the Mojo version of
  // VideoFrameMetadata because it should not cross process boundaries.
  std::optional<unsigned int> hw_va_protected_session_id;
#endif

  // An UnguessableToken that identifies VideoOverlayFactory that created
  // this VideoFrame. It's used by Cast to help with video hole punch.
  std::optional<base::UnguessableToken> overlay_plane_id;

  // Whether this frame was decoded in a power efficient way.
  bool power_efficient = false;

  // Implemented only for single texture backed frames, true means the origin of
  // the texture is top left and false means bottom left.
  bool texture_origin_is_top_left = true;

  // CompositorFrameMetadata variables associated with this frame. Used for
  // remote debugging.
  // TODO(crbug.com/40571471): Use a customized dictionary value instead of
  // using these keys directly.
  std::optional<double> device_scale_factor;
  std::optional<double> page_scale_factor;
  std::optional<double> root_scroll_offset_x;
  std::optional<double> root_scroll_offset_y;
  std::optional<double> top_controls_visible_height;

  // If present, this field represents the local time at which the VideoFrame
  // was decoded from whichever format it was encoded in. Sometimes only
  // DECODE_END_TIME will be present.
  std::optional<base::TimeTicks> decode_begin_time;
  std::optional<base::TimeTicks> decode_end_time;

  // If present, this field represents the elapsed time from the submission of
  // the encoded packet with the same PTS as this frame to the decoder until
  // the decoded frame was ready for presentation.
  std::optional<base::TimeDelta> processing_time;

  // The RTP timestamp associated with this video frame. Stored as a double
  // since base::Value::Dict doesn't have a uint32_t type.
  //
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource-rtptimestamp
  std::optional<double> rtp_timestamp;

  // For video frames coming from a remote source, this is the time the
  // encoded frame was received by the platform, i.e., the time at
  // which the last packet belonging to this frame was received over the
  // network.
  std::optional<base::TimeTicks> receive_time;

  // If present, this field represents the duration this frame is ideally
  // expected to spend on the screen during playback. Unlike FRAME_DURATION
  // this field takes into account current playback rate.
  std::optional<base::TimeDelta> wallclock_frame_duration;

  // WebRTC streams only: if present, this field represents the maximum
  // composition delay that is allowed for this frame. This is respected
  // in a best effort manner.
  // This is an experimental feature, see crbug.com/1138888 for more
  // information.
  std::optional<int> maximum_composition_delay_in_frames;

  // Identifies a BeginFrameArgs (along with the source_id).
  // See comments in components/viz/common/frame_sinks/begin_frame_args.h.
  //
  // Only set for video frames produced by the frame sink video capturer.
  std::optional<uint64_t> frame_sequence;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
