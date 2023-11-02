// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/mediastream/video_renderer_algorithm_wrapper.h"

namespace blink {

VideoRendererAlgorithmWrapper::VideoRendererAlgorithmWrapper(
    const media::TimeSource::WallClockTimeCB& wall_clock_time_cb,
    media::MediaLog* media_log)
    : wall_clock_time_cb_(wall_clock_time_cb),
      media_log_(media_log),
      renderer_algorithm_(RendererAlgorithm::kDefault) {
  default_rendering_frame_buffer_ =
      std::make_unique<media::VideoRendererAlgorithm>(wall_clock_time_cb_,
                                                      media_log_);
}

scoped_refptr<media::VideoFrame> VideoRendererAlgorithmWrapper::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    size_t* frames_dropped) {
  return renderer_algorithm_ == RendererAlgorithm::kDefault
             ? default_rendering_frame_buffer_->Render(
                   deadline_min, deadline_max, frames_dropped)
             : low_latency_rendering_frame_buffer_->Render(
                   deadline_min, deadline_max, frames_dropped);
}

void VideoRendererAlgorithmWrapper::EnqueueFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(frame);
  if (renderer_algorithm_ == RendererAlgorithm::kDefault &&
      frame->metadata().maximum_composition_delay_in_frames) {
    default_rendering_frame_buffer_.release();
    low_latency_rendering_frame_buffer_ =
        std::make_unique<LowLatencyVideoRendererAlgorithm>(media_log_);
    renderer_algorithm_ = RendererAlgorithm::kLowLatency;
  }
  return renderer_algorithm_ == RendererAlgorithm::kDefault
             ? default_rendering_frame_buffer_->EnqueueFrame(frame)
             : low_latency_rendering_frame_buffer_->EnqueueFrame(frame);
}

void VideoRendererAlgorithmWrapper::Reset(
    media::VideoRendererAlgorithm::ResetFlag reset_flag) {
  return renderer_algorithm_ == RendererAlgorithm::kDefault
             ? default_rendering_frame_buffer_->Reset(reset_flag)
             : low_latency_rendering_frame_buffer_->Reset();
}

bool VideoRendererAlgorithmWrapper::NeedsReferenceTime() const {
  return renderer_algorithm_ == RendererAlgorithm::kDefault;
}

}  // namespace blink
