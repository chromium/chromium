// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

namespace content {

namespace {

// Note on lifetime: this context should be deleted when the WebContents
// is destroyed.
class WebContentsContext : public WebContentsFrameTracker::Context {
 public:
  explicit WebContentsContext(WebContents* contents) : contents_(contents) {}
  ~WebContentsContext() override = default;

  // WebContextFrameTracker::Context overrides.
  absl::optional<gfx::Rect> GetScreenBounds() override {
    if (auto* view = GetCurrentView()) {
      // If we know the available size of the screen, we don't want to exceed
      // it as it may result in strange capture behavior in some cases.
      return view->GetScreenInfo().rect;
    }
    return absl::nullopt;
  }

  viz::FrameSinkId GetFrameSinkIdForCapture() override {
    return static_cast<WebContentsImpl*>(contents_)->GetCaptureFrameSinkId();
  }

  void IncrementCapturerCount(const gfx::Size& capture_size) override {
    capture_handle_ =
        contents_->IncrementCapturerCount(capture_size, /*stay_hidden=*/false,
                                          /*stay_awake=*/true);
  }

  void DecrementCapturerCount() override { capture_handle_.RunAndReset(); }

  void SetScaleOverrideForCapture(float scale) override {
    if (auto* view = GetCurrentView()) {
      view->SetScaleOverrideForCapture(scale);
    }
  }

  float GetScaleOverrideForCapture() const override {
    if (auto* view = GetCurrentView()) {
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
  raw_ptr<WebContents> contents_;
};

}  // namespace

WebContentsFrameTracker::WebContentsFrameTracker(
    base::WeakPtr<WebContentsVideoCaptureDevice> device,
    MouseCursorOverlayController* cursor_controller)
    : device_(std::move(device)),
      device_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(device_task_runner_);

#if !BUILDFLAG(IS_ANDROID)
  cursor_controller_ = cursor_controller;
  DCHECK(cursor_controller_);
#endif
}

WebContentsFrameTracker::~WebContentsFrameTracker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_capturing_) {
    DidStopCapturingWebContents();
  }
}

void WebContentsFrameTracker::WillStartCapturingWebContents(
    const gfx::Size& capture_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_capturing_);
  if (!web_contents()) {
    return;
  }

  capture_size_ = capture_size;
  context_->IncrementCapturerCount(CalculatePreferredSize(capture_size));
  is_capturing_ = true;
}

void WebContentsFrameTracker::DidStopCapturingWebContents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (web_contents()) {
    SetCaptureScaleOverride(1.0f);
    DCHECK(is_capturing_);
    context_->DecrementCapturerCount();
    is_capturing_ = false;
  }
  DCHECK(!is_capturing_);
}

void WebContentsFrameTracker::SetCapturedContentSize(
    const gfx::Size& content_size) {
  DVLOG(3) << __func__ << ": content_size=" << content_size.ToString();
  if (base::FeatureList::IsEnabled(media::kWebContentsCaptureHiDpi)) {
    // Check if the capture scale needs to be modified. The content_size
    // provided here is the final pixel size, with all scale factors such as the
    // device scale factor and HiDPI capture scale already applied.
    //
    // The initial content_size received here corresponds to the size of the
    // browser tab. If region capture is active, there will be an additional
    // call providing the region size. Lastly, if the scale was modified, there
    // will be another call with the upscaled size.
    SetCaptureScaleOverride(CalculatePreferredScaleFactor(content_size));
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
  if (capture_size.IsEmpty()) {
    // NOTE: An empty preferred size will cause the WebContents to keep its
    // previous size preference.
    return {};
  }
  gfx::Size preferred_size = capture_size;

  // If we know the available size of the screen, we don't want to exceed
  // it as it may result in strange capture behavior in some cases.
  if (context_) {
    const absl::optional<gfx::Rect> screen_bounds = context_->GetScreenBounds();
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
    }
  }
  return preferred_size;
}

// TODO(https://crbug.com/1329704): this should also include live updates
// about system resource availability. Perhaps we can use FPS or the
// lossyness of outputted frames?
float WebContentsFrameTracker::CalculatePreferredScaleFactor(
    const gfx::Size& content_size) {
  // A max factor above 2.0 would cause a quality degradation for local
  // rendering. The downscaling used by the compositor uses a linear filter
  // which only looks at 4 source pixels, so rendering more than 4 pixels per
  // destination pixel would result in information loss.
  constexpr float kMaxFactor = 2.0f;

  // A minimum scale factor of less than 1.0 doesn't really make any sense: this
  // would only occur if the "preferred size" is larger than the specified
  // capture size, which should never happen.
  constexpr float kMinFactor = 1.0f;

  // Ideally is that the |content_size| is the same as |capture_size_|, so if
  // we are achieving that with current settings we can exit early.
  if (content_size.width() == capture_size_.width() &&
      content_size.height() == capture_size_.height()) {
    return capture_scale_override_;
  }

  // The content_size should already be scaled based on the currently set
  // scale factor, so start by looking at what the content size would have been
  // if scaling was not enabled.
  const auto unscaled_content_size = gfx::ScaleToCeiledSize(
      content_size, 1.0f / context_->GetScaleOverrideForCapture());

  // Next, determine what the ideal scale factors in each direction would have
  // been for this frame.
  const gfx::Vector2dF factors(
      static_cast<float>(capture_size_.width()) / unscaled_content_size.width(),
      static_cast<float>(capture_size_.height()) /
          unscaled_content_size.height());

  // We prefer to err on the side of having to downscale in one direction rather
  // than upscale in the other direction, so we use the largest scale factor.
  const float largest_factor = std::max(factors.x(), factors.y());
  return std::clamp(largest_factor, kMinFactor, kMaxFactor);
}

void WebContentsFrameTracker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  OnPossibleTargetChange();
  if (capture_scale_override_ != 1.0f) {
    if (auto* view = render_frame_host->GetView()) {
      // Inside content, down-casting from the public interface class is safe.
      static_cast<RenderWidgetHostViewBase*>(view)->SetScaleOverrideForCapture(
          capture_scale_override_);
    }
  }
}

void WebContentsFrameTracker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  OnPossibleTargetChange();
  if (capture_scale_override_ != 1.0f) {
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
          ->SetScaleOverrideForCapture(capture_scale_override_);
    }
  }
}

void WebContentsFrameTracker::WebContentsDestroyed() {
  is_capturing_ = false;
  Observe(nullptr);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::CaptureTargetChanged() {
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId(
    const GlobalRenderFrameHostId& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Observe(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(id)));
  context_ = std::make_unique<WebContentsContext>(web_contents());
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback);

  if (crop_version_ >= crop_version) {
    // This will trigger a BadMessage from MediaStreamDispatcherHost.
    // (MediaStreamDispatcherHost knows the capturer, whereas here we know
    // the capturee.)
    std::move(callback).Run(
        media::mojom::CropRequestResult::kNonIncreasingCropVersion);
    return;
  }

  crop_id_ = crop_id;
  crop_version_ = crop_version;

  // If we don't have a target yet, we can store the crop ID but cannot actually
  // crop yet.
  if (!target_frame_sink_id_.is_valid())
    return;

  const viz::VideoCaptureTarget target(target_frame_sink_id_, crop_id_);
  device_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const viz::VideoCaptureTarget& target, uint32_t crop_version,
             base::OnceCallback<void(media::mojom::CropRequestResult)> callback,
             base::WeakPtr<WebContentsVideoCaptureDevice> device) {
            if (!device) {
              std::move(callback).Run(
                  media::mojom::CropRequestResult::kErrorGeneric);
              return;
            }
            device->OnTargetChanged(target, crop_version);
            std::move(callback).Run(media::mojom::CropRequestResult::kSuccess);
          },
          target, crop_version_, std::move(callback), device_));
}

void WebContentsFrameTracker::SetWebContentsAndContextForTesting(
    WebContents* web_contents,
    std::unique_ptr<WebContentsFrameTracker::Context> context) {
  Observe(web_contents);
  context_ = std::move(context);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::OnPossibleTargetChange() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!web_contents()) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetPermanentlyLost,
                       device_));
    SetTargetView({});
    return;
  }

  viz::FrameSinkId frame_sink_id;
  if (context_) {
    frame_sink_id = context_->GetFrameSinkIdForCapture();
  }

  // TODO(crbug.com/1247761): Clear |crop_id_| when share-this-tab-instead
  // is clicked.
  if (frame_sink_id != target_frame_sink_id_) {
    target_frame_sink_id_ = frame_sink_id;
    absl::optional<viz::VideoCaptureTarget> target;
    if (frame_sink_id.is_valid()) {
      target = viz::VideoCaptureTarget(frame_sink_id, crop_id_);
    }

    // The target may change to an invalid one, but we don't consider it
    // permanently lost here yet.
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContentsVideoCaptureDevice::OnTargetChanged, device_,
                       std::move(target), crop_version_));
  }

  SetTargetView(web_contents()->GetNativeView());
}

// Note: MouseCursorOverlayController runs on the UI thread. It's also
// important that SetTargetView() be called in the current stack while
// |view| is known to be a valid pointer. http://crbug.com/818679
void WebContentsFrameTracker::SetTargetView(gfx::NativeView view) {
  if (view == target_native_view_)
    return;
  target_native_view_ = view;
#if !BUILDFLAG(IS_ANDROID)
  cursor_controller_->SetTargetView(view);
#endif
}

void WebContentsFrameTracker::SetCaptureScaleOverride(float new_value) {
  if (new_value != capture_scale_override_) {
    capture_scale_override_ = new_value;
    context_->SetScaleOverrideForCapture(new_value);
  }
}
}  // namespace content
