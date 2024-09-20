// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

namespace content {

namespace {

// A minimum factor of 1.0 means that no DPI scaling is applied.
static constexpr float kMinCaptureScaleOverride = 1.0;

// Note on lifetime: this context is deleted via WebContentsObserver's
// WebContentsDestroyed() method when the WebContents is destroyed.
class WebContentsContext : public WebContentsFrameTracker::Context {
 public:
  explicit WebContentsContext(WebContents* contents) : contents_(contents) {
    DCHECK(contents_);
  }
  ~WebContentsContext() override = default;

  // WebContextFrameTracker::Context overrides.
  std::optional<gfx::Rect> GetScreenBounds() override {
    if (auto* view = GetCurrentView()) {
      // If we know the available size of the screen, we don't want to exceed
      // it as it may result in strange capture behavior in some cases.
      return view->GetScreenInfo().rect;
    }
    return std::nullopt;
  }

  WebContentsImpl::CaptureTarget GetCaptureTarget() override {
    return static_cast<WebContentsImpl*>(contents_)->GetCaptureTarget();
  }

  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    capture_handle_ = contents_->IncrementCapturerCount(
        capture_size, /*stay_hidden=*/false,
        /*stay_awake=*/true, /*is_activity=*/true);
  }

  void DecrementCapturerCount() override { capture_handle_.RunAndReset(); }

  void SetScaleOverrideForCapture(float scale) override {
    if (auto* view = GetCurrentView()) {
      view->SetScaleOverrideForCapture(scale);
    }
  }

  float GetScaleOverrideForCapture() const override {
    if (const auto* view = GetCurrentView()) {
      return view->GetScaleOverrideForCapture();
    }
    // Otherwise we can assume it's unset and return the default value.
    return 1.0f;
  }

 private:
  RenderWidgetHostViewBase* GetCurrentView() const {
    RenderWidgetHostView* view = contents_->GetRenderWidgetHostView();
    // Make sure the RWHV is still associated with a RWH before considering the
    // view "alive." This is because a null RWH indicates the RWHV has had its
    // Destroy() method called.
    if (!view || !view->GetRenderWidgetHost()) {
      return nullptr;
    }
    // Inside content, down-casting from the public interface class is safe.
    return static_cast<RenderWidgetHostViewBase*>(view);
  }

  base::ScopedClosureRunner capture_handle_;

  // The backing WebContents.
  raw_ptr<WebContents, DanglingUntriaged> contents_;
};

}  // namespace

// A max factor above 2.0 would cause a quality degradation for local
// rendering. The downscaling used by the compositor uses a linear filter
// which only looks at 4 source pixels, so rendering more than 4 pixels per
// destination pixel would result in information loss.
//
// static
const float WebContentsFrameTracker::kMaxCaptureScaleOverride = 2.0f;

WebContentsFrameTracker::WebContentsFrameTracker(
    scoped_refptr<base::SequencedTaskRunner> device_task_runner,
    base::WeakPtr<WebContentsVideoCaptureDevice> device,
    MouseCursorOverlayController* cursor_controller)
    : device_(std::move(device)),
      device_task_runner_(std::move(device_task_runner))
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      ,
      cursor_controller_(cursor_controller->GetWeakPtr())
#endif
{
  // Verify on construction that this object is created on the UI thread.  After
  // this, depend on the sequence checker to ensure consistent execution.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(device_task_runner_);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  CHECK(cursor_controller_);
#endif
}

WebContentsFrameTracker::~WebContentsFrameTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_capturing_) {
    DidStopCapturingWebContents();
  }
}

void WebContentsFrameTracker::WillStartCapturingWebContents(
    const gfx::Size& capture_size,
    bool is_high_dpi_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_capturing_);
  if (!web_contents()) {
    return;
  }

  capture_size_ = capture_size;
  is_high_dpi_enabled_ =
      is_high_dpi_enabled &&
      base::FeatureList::IsEnabled(media::kWebContentsCaptureHiDpi) &&
      !GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled();
  context_->IncrementCapturerCount(CalculatePreferredSize(capture_size));
  is_capturing_ = true;
}

void WebContentsFrameTracker::DidStopCapturingWebContents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents()) {
    SetCaptureScaleOverride(1.0f);
    DCHECK(is_capturing_);
    context_->DecrementCapturerCount();
    is_capturing_ = false;

    UMA_HISTOGRAM_COUNTS_1000("Media.VideoCapture.ScaleOverrideChangeCount",
                              scale_override_change_count_);
    scale_override_change_count_ = 0;
  }
  DCHECK(!is_capturing_);
}

void WebContentsFrameTracker::SetCapturedContentSize(
    const gfx::Size& content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!web_contents()) {
    return;
  }

  // For efficiency, this function should only be called when the captured
  // content size changes. The caller is responsible for enforcing that.
  TRACE_EVENT_INSTANT1(
      "gpu.capture", "WebContentsFrameTracker::SetCapturedContentSize",
      TRACE_EVENT_SCOPE_THREAD, "content_size", content_size.ToString());

  if (is_high_dpi_enabled_) {
    // Now that we have a new content size, reset some related values.
    content_size_ = content_size;
    max_capture_scale_override_ = kMaxCaptureScaleOverride;

    // The unscaled content size can be determined by removing the scale factor
    // from the |content_size|.
    const float scale_override = context_->GetScaleOverrideForCapture();
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
}

// We provide the WebContents with a preferred size override during its capture.
// The preferred size is a strong suggestion to UI layout code to size the view
// such that its physical rendering size matches the exact capture size. This
// helps to eliminate redundant scaling operations during capture. Note that if
// there are multiple capturers, a "first past the post" system is used and
// the first capturer's preferred size is set.
gfx::Size WebContentsFrameTracker::CalculatePreferredSize(
    const gfx::Size& capture_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capture_size.IsEmpty()) {
    // NOTE: An empty preferred size will cause the WebContents to keep its
    // previous size preference.
    return {};
  }
  gfx::Size preferred_size = capture_size;

  // If we know the available size of the screen, we don't want to exceed
  // it as it may result in strange capture behavior in some cases.
  if (context_) {
    const std::optional<gfx::Rect> screen_bounds = context_->GetScreenBounds();
    if (screen_bounds) {
      if (screen_bounds->size().IsEmpty()) {
        return {};
      }

      // We want to honor the aspect ratio of the capture size request while
      // also limiting it to the screen bounds of the view.
      // For motivation, see https://crbug.com/1194803.
      const double x_ratio = static_cast<double>(capture_size.width()) /
                             static_cast<double>(screen_bounds->size().width());
      const double y_ratio =
          static_cast<double>(capture_size.height()) /
          static_cast<double>(screen_bounds->size().height());

      const double scale_ratio = std::max(x_ratio, y_ratio);
      if (scale_ratio > 1.0) {
        preferred_size = gfx::ScaleToFlooredSize(
            preferred_size, static_cast<float>(1 / scale_ratio));
      }

      DVLOG(3) << __func__ << ": x_ratio=" << x_ratio << " y_ratio=" << y_ratio
               << " scale_ratio=" << scale_ratio
               << " preferred_size=" << preferred_size.ToString();
    }
  }
  return preferred_size;
}

float WebContentsFrameTracker::CalculatePreferredScaleFactor(
    const gfx::Size& current_content_size,
    const gfx::Size& unscaled_current_content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_high_dpi_enabled_);

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

  DVLOG(3) << __func__ << ":"
           << " capture_size_=" << capture_size_.ToString()
           << ", letterbox_size=" << letterbox_size.ToString()
           << ", current_content_size=" << current_content_size.ToString()
           << ", unscaled_current_content_size="
           << unscaled_current_content_size.ToString()
           << ", factors.x()=" << factors.x() << " factors.y()=" << factors.y()
           << ", largest_factor=" << largest_factor
           << ", preferred factor=" << preferred_factor;
  return preferred_factor;
}

void WebContentsFrameTracker::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  TRACE_EVENT_INSTANT2(
      "gpu.capture", "WebContentsFrameTracker::OnUtilizationReport",
      TRACE_EVENT_SCOPE_THREAD, "utilization", feedback.resource_utilization,
      "max_pixels", feedback.max_pixels);

  capture_feedback_ = std::move(feedback);

  // We may not be associated with a web contents when we get a report, but
  // it is still valid and potentially interesting.
  if (context_) {
    SetCaptureScaleOverride(desired_capture_scale_override_);
  }
}

void WebContentsFrameTracker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
  if (desired_capture_scale_override_ != 1.0f) {
    if (auto* view = render_frame_host->GetView()) {
      // Inside content, down-casting from the public interface class is safe.
      static_cast<RenderWidgetHostViewBase*>(view)->SetScaleOverrideForCapture(
          desired_capture_scale_override_);
    }
  }
}

void WebContentsFrameTracker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
  if (desired_capture_scale_override_ != 1.0f) {
    // According to WebContentsObserver docs, old_host can be nullptr.
    if (old_host) {
      if (auto* old_view = old_host->GetView()) {
        // Inside content, down-casting from the public interface class is safe.
        static_cast<RenderWidgetHostViewBase*>(old_view)
            ->SetScaleOverrideForCapture(1.0f);
      }
    }
    if (auto* new_view = new_host->GetView()) {
      static_cast<RenderWidgetHostViewBase*>(new_view)
          ->SetScaleOverrideForCapture(desired_capture_scale_override_);
    }
  }
}

void WebContentsFrameTracker::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_ = false;
  context_ = nullptr;
  Observe(nullptr);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::CaptureTargetChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId(
    const GlobalRenderFrameHostId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Observe(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(id)));
  if (web_contents()) {
    // If the routing ID was invalid, don't set up a context.
    context_ = std::make_unique<WebContentsContext>(web_contents());
  }
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::ApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type,
    const base::Token& target_token,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (sub_capture_target_version_ >= sub_capture_target_version) {
    // This will trigger a BadMessage from MediaStreamDispatcherHost.
    // (MediaStreamDispatcherHost knows the capturer, whereas here we know
    // the capturee.)
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kNonIncreasingVersion);
    return;
  }

  sub_capture_target_ =
      target_token.is_zero()
          ? std::nullopt
          : std::make_optional<SubCaptureTargetInfo>(type, target_token);

  sub_capture_target_version_ = sub_capture_target_version;

  // If we don't have a target yet, we can store the sub-capture target,
  // but cannot actually apply it yet.
  if (!target_frame_sink_id_.is_valid()) {
    return;
  }

  const viz::VideoCaptureTarget target(target_frame_sink_id_,
                                       DeriveSubTarget());
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const viz::VideoCaptureTarget& target,
             uint32_t sub_capture_target_version,
             base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
                 callback,
             base::WeakPtr<WebContentsVideoCaptureDevice> device) {
            if (!device) {
              std::move(callback).Run(
                  media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
              return;
            }
            device->OnTargetChanged(target, sub_capture_target_version);
            std::move(callback).Run(
                media::mojom::ApplySubCaptureTargetResult::kSuccess);
          },
          target, sub_capture_target_version_, std::move(callback), device_));
}

void WebContentsFrameTracker::SetWebContentsAndContextForTesting(
    WebContents* web_contents,
    std::unique_ptr<WebContentsFrameTracker::Context> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Observe(web_contents);
  context_ = std::move(context);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::OnPossibleTargetChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents()) {
    DCHECK(!context_);
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    SetTargetView({});
    return;
  }

  const WebContentsImpl::CaptureTarget capture_target =
      context_ ? context_->GetCaptureTarget()
               : WebContentsImpl::CaptureTarget{};

  // TODO(crbug.com/40203554): Clear |sub_capture_target_| when
  // share-this-tab-instead is clicked.
  if (capture_target.sink_id != target_frame_sink_id_) {
    target_frame_sink_id_ = capture_target.sink_id;
    std::optional<viz::VideoCaptureTarget> target;
    if (capture_target.sink_id.is_valid()) {
      target =
          viz::VideoCaptureTarget(capture_target.sink_id, DeriveSubTarget());
    }

    // The target may change to an invalid one, but we don't consider it
    // permanently lost here yet.
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetChanged, device_,
                       std::move(target), sub_capture_target_version_));
  }

  // Note: MouseCursorOverlayController runs on the UI thread. SetTargetView()
  // must be called synchronously since the NativeView pointer is not valid
  // across task switches, cf. https://crbug.com/818679
  SetTargetView(capture_target.view);
}

void WebContentsFrameTracker::SetTargetView(gfx::NativeView view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (view == target_native_view_) {
    return;
  }
  target_native_view_ = view;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (cursor_controller_) {
    cursor_controller_->SetTargetView(view);
  }
#endif
}

void WebContentsFrameTracker::SetCaptureScaleOverride(float new_value) {
  DCHECK(context_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, record the desired value for future lookup.
  desired_capture_scale_override_ = new_value;

  // Then, if the value adjusted by max is not the same as the current value,
  // apply it to the context.
  const float current_value = context_->GetScaleOverrideForCapture();
  const float bounded_value = std::min(new_value, DetermineMaxScaleOverride());
  if (bounded_value != current_value) {
    context_->SetScaleOverrideForCapture(bounded_value);

    ++scale_override_change_count_;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.VideoCapture.ScaleOverride",
                                new_value * 100, kMinCaptureScaleOverride * 100,
                                kMaxCaptureScaleOverride * 100 + 1, 50);
  }
}

float WebContentsFrameTracker::DetermineMaxScaleOverride() {
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
      "gpu.capture", "WebContentsFrameTracker::DetermineMaxScaleOverride",
      TRACE_EVENT_SCOPE_THREAD, "max_scale_override",
      max_capture_scale_override_, "constraints",
      base::StrCat(
          {"max_pixels=", base::NumberToString(capture_feedback_->max_pixels),
           ", utilization=",
           base::NumberToString(capture_feedback_->resource_utilization)}));
  return max_capture_scale_override_;
}

viz::VideoCaptureSubTarget WebContentsFrameTracker::DeriveSubTarget() const {
  if (!sub_capture_target_.has_value()) {
    return base::Token();
  }

  const SubCaptureTargetInfo& sub_capture_target = sub_capture_target_.value();
  switch (sub_capture_target.type) {
    case media::mojom::SubCaptureTargetType::kCropTarget:
      return sub_capture_target.token;
    case media::mojom::SubCaptureTargetType::kRestrictionTarget:
      return viz::SubtreeCaptureId(sub_capture_target.token);
  }

  NOTREACHED();
}

}  // namespace content
