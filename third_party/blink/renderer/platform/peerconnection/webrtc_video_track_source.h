// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_VIDEO_TRACK_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_VIDEO_TRACK_SOURCE_H_

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/video/video_capture_feedback.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/media/base/adapted_video_track_source.h"
#include "third_party/webrtc/rtc_base/timestamp_aligner.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace blink {

// This class implements webrtc's VideoTrackSourceInterface. To pass frames down
// the webrtc video pipeline, each received a media::VideoFrame is converted to
// a webrtc::VideoFrame, taking any adaptation requested by downstream classes
// into account.
class PLATFORM_EXPORT WebRtcVideoTrackSource
    : public rtc::AdaptedVideoTrackSource {
 public:
  struct FrameAdaptationParams {
    bool should_drop_frame;
    int crop_x;
    int crop_y;
    int crop_width;
    int crop_height;
    int scale_to_width;
    int scale_to_height;
  };

  WebRtcVideoTrackSource(bool is_screencast,
                         absl::optional<bool> needs_denoising,
                         media::VideoCaptureFeedbackCB feedback_callback,
                         base::RepeatingClosure request_refresh_frame_callback,
                         media::GpuVideoAcceleratorFactories* gpu_factories);
  WebRtcVideoTrackSource(const WebRtcVideoTrackSource&) = delete;
  WebRtcVideoTrackSource& operator=(const WebRtcVideoTrackSource&) = delete;
  ~WebRtcVideoTrackSource() override;

  void SetCustomFrameAdaptationParamsForTesting(
      const FrameAdaptationParams& params);

  void SetSinkWantsForTesting(const rtc::VideoSinkWants& sink_wants);

  SourceState state() const override;

  bool remote() const override;
  bool is_screencast() const override;
  absl::optional<bool> needs_denoising() const override;
  void OnFrameCaptured(
      scoped_refptr<media::VideoFrame> frame,
      std::vector<scoped_refptr<media::VideoFrame>> scaled_frames);
  void OnNotifyFrameDropped();

  using webrtc::VideoTrackSourceInterface::AddOrUpdateSink;
  using webrtc::VideoTrackSourceInterface::RemoveSink;
  void RequestRefreshFrame() override;

 private:
  void SendFeedback();

  FrameAdaptationParams ComputeAdaptationParams(int width,
                                                int height,
                                                int64_t time_us);

  // Delivers |frame| to base class method
  // rtc::AdaptedVideoTrackSource::OnFrame(). If the cropping (given via
  // |frame->visible_rect()|) has changed since the last delivered frame, the
  // whole frame is marked as updated.
  // |timestamp_us| is |frame->timestamp()| in Microseconds but clipped to
  // ensure that it doesn't exceed the current system time. However,
  // |capture_time_identifier| is just |frame->timestamp()|.
  void DeliverFrame(scoped_refptr<media::VideoFrame> frame,
                    std::vector<scoped_refptr<media::VideoFrame>> scaled_frames,
                    gfx::Rect* update_rect,
                    int64_t timestamp_us,
                    absl::optional<webrtc::Timestamp> capture_time_identifier);

  // This checks if the colorspace information should be passed to webrtc. Avoid
  // sending unknown or unnecessary color space.
  bool ShouldSetColorSpace(const gfx::ColorSpace& color_space);

  // |thread_checker_| is bound to the libjingle worker thread.
  THREAD_CHECKER(thread_checker_);
  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> adapter_resources_;
  // State for the timestamp translation.
  rtc::TimestampAligner timestamp_aligner_;

  const bool is_screencast_;
  const absl::optional<bool> needs_denoising_;

  // Stores the accumulated value of CAPTURE_UPDATE_RECT in case that frames
  // are dropped.
  absl::optional<gfx::Rect> accumulated_update_rect_;
  absl::optional<int> previous_capture_counter_;
  gfx::Rect cropping_rect_of_previous_delivered_frame_;
  gfx::Size natural_size_of_previous_delivered_frame_;

  absl::optional<FrameAdaptationParams>
      custom_frame_adaptation_params_for_testing_;

  const media::VideoCaptureFeedbackCB feedback_callback_;
  const base::RepeatingClosure request_refresh_frame_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_VIDEO_TRACK_SOURCE_H_
