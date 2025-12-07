// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_auto_scaler.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {

namespace {
// A minimum factor of 1.0 means that no DPI scaling is applied.
static constexpr float kMinCaptureScaleOverride = 1.0;
}  // namespace

WebContentsAutoScaler::Delegate::Delegate() = default;
WebContentsAutoScaler::Delegate::~Delegate() = default;

WebContentsAutoScaler::WebContentsAutoScaler(Delegate& delegate,
                                             const gfx::Size& capture_size)
    : delegate_(delegate), capture_size_(capture_size) {}
WebContentsAutoScaler::~WebContentsAutoScaler() = default;

void WebContentsAutoScaler::SetCapturedContentSize(
    const gfx::Size& content_size) {
  // Now that we have a new content size, reset some related values.
  content_size_ = content_size;
  max_capture_scale_override_ = kMaxCaptureScaleOverride;

  // The unscaled content size can be determined by removing the scale factor
  // from the |content_size|.
  const float scale_override = delegate_->GetCaptureScaleOverride();
  DCHECK_NE(0.0f, scale_override);
  const gfx::Size unscaled_content_size =
      gfx::ScaleToCeiledSize(content_size, 1.0f / scale_override);

  // Check if the capture scale needs to be modified. The content_size
  // provided here is the final pixel size, with all scale factors such as the
  // device scale factor and HiDPI capture scale already applied.
  //
  // The initial content_size received here corresponds to the size of the
  // browser tab. If region capture is active, there will be an additional
  // call providing the region size. Lastly, if the scale was modified, there
  // will be another call with the upscaled size.
  const float factor =
      CalculatePreferredScaleFactor(content_size, unscaled_content_size);
  SetCaptureScaleOverride(factor);
}

void WebContentsAutoScaler::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  capture_feedback_ = std::move(feedback);
  // Does not actually update the desired value (which is based on the content
  // size and capture size), but may lower the current scale factor by capping
  // it to a lower `max_capture_scale_override_` after processing the feedback.
  SetCaptureScaleOverride(desired_capture_scale_override_);
}

float WebContentsAutoScaler::GetDesiredCaptureScaleOverride() const {
  return desired_capture_scale_override_;
}

int WebContentsAutoScaler::GetScaleOverrideChangeCount() const {
  return scale_override_change_count_;
}

void WebContentsAutoScaler::SetCaptureScaleOverride(float new_value) {
  // First, record the desired value for future lookup.
  desired_capture_scale_override_ = new_value;

  // Then, if the value adjusted by max is not the same as the current value,
  // apply it to the context.
  const float current_value = delegate_->GetCaptureScaleOverride();
  const float bounded_value = std::min(new_value, DetermineMaxScaleOverride());
  if (bounded_value != current_value) {
    delegate_->SetCaptureScaleOverride(bounded_value);

    ++scale_override_change_count_;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.VideoCapture.ScaleOverride",
                                new_value * 100, kMinCaptureScaleOverride * 100,
                                kMaxCaptureScaleOverride * 100 + 1, 50);
  }
}

float WebContentsAutoScaler::CalculatePreferredScaleFactor(
    const gfx::Size& current_content_size,
    const gfx::Size& unscaled_current_content_size) {
  // The content size does not include letterboxing, meaning that there may
  // be an aspect ratio difference between the content size and the final
  // capture size. For example, if the video frame consumer requests a 1080P
  // video stream and the web contents has a size of 960x720 (ratio of 4:3), the
  // letterboxed size here will be 1440x1080 (still 4:3). Graphically:
  //
  //    |capture_size_|
  //    |----------------------------------------------------|
  //    |    | |letterbox_size|                         | .  |
  //    |    |     |-------------------------------|    |    |
  //    |    |     | |content_size|                |    |    |
  //    |    |     |-------------------------------|    |    |
  //    |    |                                          |    |
  //    |----------------------------------------------------|
  //
  // In order to preserve the aspect ratio of the web contents, we use this
  // letterboxed size with the same aspect ratio instead of the requested
  // capture size's aspect ratio.
  gfx::Size letterbox_size =
      media::ComputeLetterboxRegion(gfx::Rect(capture_size_),
                                    unscaled_current_content_size)
          .size();

  // Ideally the |current_content_size| should be the same as |letterbox_size|,
  // so if we are achieving that with current settings we can exit early. Since
  // we only scale by factors of 1/4, we accept a difference here of up to 1/8th
  // of the letterboxed size, meaning that this scale factor would have been a
  // more appropriate fit that a neighboring factor.
  if (std::abs(current_content_size.width() - letterbox_size.width()) <=
          (letterbox_size.width() / 8) &&
      std::abs(current_content_size.height() - letterbox_size.height()) <=
          (letterbox_size.height() / 8)) {
    return desired_capture_scale_override_;
  }

  // Next, determine what the ideal scale factors in each direction would have
  // been for this frame. Since we are using the letterboxed size here, the
  // factors should be almost identical.
  DCHECK_NE(0.0f, unscaled_current_content_size.width());
  DCHECK_NE(0.0f, unscaled_current_content_size.height());
  const gfx::Vector2dF factors(static_cast<float>(letterbox_size.width()) /
                                   unscaled_current_content_size.width(),
                               static_cast<float>(letterbox_size.height()) /
                                   unscaled_current_content_size.height());

  // We prefer to err on the side of having to downscale in one direction rather
  // than upscale in the other direction, so we use the largest scale factor.
  const float largest_factor = std::max(factors.x(), factors.y());

  // Finally, we return a value bounded by [kMinCaptureScaleOverride,
  // kMaxCaptureScaleOverride] rounded to the nearest quarter.
  const float preferred_factor =
      std::clamp(std::round(largest_factor * 4) / 4, kMinCaptureScaleOverride,
                 kMaxCaptureScaleOverride);

  DVLOG(3) << __func__ << ":" << " capture_size_=" << capture_size_.ToString()
           << ", letterbox_size=" << letterbox_size.ToString()
           << ", current_content_size=" << current_content_size.ToString()
           << ", unscaled_current_content_size="
           << unscaled_current_content_size.ToString()
           << ", factors.x()=" << factors.x() << " factors.y()=" << factors.y()
           << ", largest_factor=" << largest_factor
           << ", preferred factor=" << preferred_factor;
  return preferred_factor;
}

float WebContentsAutoScaler::DetermineMaxScaleOverride() {
  // If we have no feedback or don't want to apply a scale factor, leave it
  // unchanged.
  if (!capture_feedback_ || !content_size_) {
    return max_capture_scale_override_;
  }

  // First, determine if we need to lower the max scale override.
  // Clue 1: we are above 80% resource utilization.
  bool should_decrease_override =
      capture_feedback_->resource_utilization > 0.8f;

  // Clue 2: we are using too many pixels.
  if (content_size_) {
    should_decrease_override |=
        content_size_->width() * content_size_->height() >
        capture_feedback_->max_pixels;
  }

  if (should_decrease_override) {
    max_capture_scale_override_ =
        std::max(kMinCaptureScaleOverride, max_capture_scale_override_ - 0.25f);
  }

  // Second, determine if conditions have gotten better to the point where
  // we can increase the maximum scale override.
  if (!should_decrease_override &&
      max_capture_scale_override_ < kMaxCaptureScaleOverride) {
    // Clue A: using less than 40% of resources.
    bool should_increase_override =
        capture_feedback_->resource_utilization < 0.5f;

    // Clue B: we are ALSO significantly below the max pixels.
    should_increase_override &=
        content_size_->width() * content_size_->height() <
        capture_feedback_->max_pixels * 0.8;

    if (should_increase_override) {
      max_capture_scale_override_ = std::min(
          kMaxCaptureScaleOverride, max_capture_scale_override_ + 0.25f);
    }
  }

  TRACE_EVENT_INSTANT2(
      "gpu.capture", "WebContentsAutoScaler::DetermineMaxScaleOverride",
      TRACE_EVENT_SCOPE_THREAD, "max_scale_override",
      max_capture_scale_override_, "constraints",
      base::StrCat(
          {"max_pixels=", base::NumberToString(capture_feedback_->max_pixels),
           ", utilization=",
           base::NumberToString(capture_feedback_->resource_utilization)}));
  return max_capture_scale_override_;
}

// A max factor above 2.0 would cause a quality degradation for local
// rendering. The downscaling used by the compositor uses a linear filter
// which only looks at 4 source pixels, so rendering more than 4 pixels per
// destination pixel would result in information loss.
// static
const float WebContentsAutoScaler::kMaxCaptureScaleOverride = 2.0f;

}  // namespace content
