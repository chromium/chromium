// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/capture_version.h"
#include "media/base/media_export.h"
#include "media/base/video_transformation.h"
#include "media/gpu/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/vulkan/vulkan_ycbcr_info.h"
#endif

namespace media {

// A container for information about effects that might be applied to a frame.
struct EffectInfo {
  bool enabled;
  bool operator==(const EffectInfo& other) const {
    return enabled == other.enabled;
  }
};

// Capture-specific counter, geometry, temporal damage, and sub-capture
// (Region/Element Capture) metadata attached to a captured `VideoFrame`.
//
// NOTE: When adding new CaptureMetadata fields, please ensure you update
// CaptureMetadata::MergeMetadataFrom() and media.mojom.CaptureMetadata.
struct MEDIA_EXPORT CaptureMetadata {
  CaptureMetadata();
  ~CaptureMetadata();

  CaptureMetadata(const CaptureMetadata& other);
  CaptureMetadata(CaptureMetadata&& other);
  CaptureMetadata& operator=(const CaptureMetadata& other);
  CaptureMetadata& operator=(CaptureMetadata&& other);

  bool operator==(const CaptureMetadata& other) const = default;

  // Merges internal values from `metadata_source`.
  void MergeMetadataFrom(const CaptureMetadata& metadata_source);

  // Monotonically increasing counter incremented by the producer for each
  // captured frame. Consumers can check for gaps between consecutive frames to
  // detect dropped frames (which invalidate `update_rect`). The initial value
  // is arbitrary.
  std::optional<int> counter;

  // Bounding rect of pixels in the coded frame (`coded_size()`) that changed
  // since the immediately preceding `counter` frame (`*counter ==
  // *prev_counter + 1`). Used by video encoders to skip unchanged regions.
  // If `counter` has a gap, consumers must ignore `update_rect` and treat the
  // entire frame as updated.
  std::optional<gfx::Rect> update_rect;

  // Unscaled pixel size of the captured source (or active cropped region when
  // GPU Region Capture cropping is active) before scaling or letterboxing into
  // the `VideoFrame`. Used to adjust capture resolution (e.g. HiDPI tab
  // capture) and compute `screenPixelRatio`.
  std::optional<gfx::Size> source_size;

  // Physical crop rect in the requested root frame sink's DIP coordinate space.
  // Set only when Viz physically crops a single-target Region Capture in the
  // root frame sink; `std::nullopt` when uncropped, in multi-target full-frame
  // mode, during Element Capture, or when cropping an out-of-process iframe
  // (https://crbug.com/1327560). Used to draw the browser's tab-sharing border
  // and to drop GPU-cropped frames on uncropped tracks.
  std::optional<gfx::Rect> region_capture_rect;

  // Maps each active `RegionCaptureCropId` (`base::Token`) to its bounding rect
  // in the delivered `VideoFrame` (`visible_rect()`):
  //   - In full-frame mode, contains scaled/letterboxed bounds for all active
  //     crop targets so Blink can crop per track via `WrapVideoFrame()`.
  //   - In single-target GPU-cropped mode, maps the active crop target to
  //     `visible_rect()`.
  base::flat_map<base::Token, gfx::Rect> region_capture_bounds;

  // The `(source, sub_capture)` version pair (`media::CaptureVersion`) for
  // this frame. `source` increments when the captured surface changes;
  // `sub_capture` increments on `cropTo()` / `restrictTo()`. Used to resolve
  // pending sub-capture promises and discard pre-crop frames.
  media::CaptureVersion version;
};

// NOTE: When adding new VideoFrameMetadata fields, please ensure you update the
// MergeMetadataFrom() method.
struct MEDIA_EXPORT VideoFrameMetadata {
  VideoFrameMetadata();
  ~VideoFrameMetadata();

  VideoFrameMetadata(const VideoFrameMetadata& other);
  VideoFrameMetadata(VideoFrameMetadata&& other);
  VideoFrameMetadata& operator=(const VideoFrameMetadata& other);
  VideoFrameMetadata& operator=(VideoFrameMetadata&& other);

  // Merges internal values from |metadata_source|.
  void MergeMetadataFrom(const VideoFrameMetadata& metadata_source);

  // Clear metadata fields that only make sense for texture backed frames.
  void ClearTextureFrameMetadata();

  // Video capture begin/end timestamps.  Consumers can use these values for
  // dynamic optimizations, logging stats, etc.
  std::optional<base::TimeTicks> capture_begin_time;
  std::optional<base::TimeTicks> capture_end_time;

  // Capture-specific geometry, temporal damage, and sub-capture (Region/Element
  // Capture) metadata. See `CaptureMetadata` above for details on how each
  // field is populated and consumed.
  CaptureMetadata capture;

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

  // Android only: For legacy overlays (SurfaceView/Dialog based) this is
  // required for the frame to be suitable for overlays. If `in_surface_view`
  // is not set, Display Compositor will process frame and generate appropriate
  // overlay promotion hints, but will still composite video.
  bool in_surface_view = false;

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

  // An UnguessableToken that can track a frame's underlying platform specific
  // resources or identify its source. For "hole" VideoFrame's, it is used to
  // identifies VideoOverlayFactory that created the frame. If this is set, it
  // should not be modified.
  std::optional<base::UnguessableToken> tracking_token;

  // Whether this frame was decoded in a power efficient way.
  bool power_efficient = false;

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
  // since base::DictValue doesn't have a uint32_t type.
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

  // Identifies a BeginFrameArgs
  // See comments in components/viz/common/frame_sinks/begin_frame_args.h.
  //
  // Only set for video frames produced by the frame sink video capturer.
  std::optional<uint64_t> frame_sequence;
  std::optional<uint64_t> source_id;

  // Information about any background blur effect applied to the frame.
  std::optional<EffectInfo> background_blur;

#if BUILDFLAG(IS_ANDROID)
  // Vulkan sampler conversion information for shared images backed by
  // multiplanar hardware buffers, such as those obtained from MediaCodec
  // or Camera2 via ImageReader.
  std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
#endif
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
