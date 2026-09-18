// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_frame_tracker.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/media/capture/web_contents_auto_scaler.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_ui_types.h"

#if !BUILDFLAG(IS_IOS)
#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"
#endif

namespace content {

namespace {

void SetScaleOverrideForCapture(RenderFrameHost* rfh, float scale_override) {
  if (rfh) {
    // Inside content, down-casting from the public interface class is safe.
    auto* view = static_cast<RenderWidgetHostViewBase*>(rfh->GetView());
    if (view) {
      view->SetScaleOverrideForCapture(scale_override);
    }
  }
}

// Remembers which WebContents a tab-capture routing ID originally resolved to.
//
// A tab capture is identified by the routing ID of the captured tab's main
// RenderFrameHost at the moment the capture was requested. That routing ID is
// baked into the DesktopMediaID and never changes for the lifetime of the
// capture session.
//
// While a capture device is alive, WebContentsFrameTracker observes the
// captured WebContents and follows it across navigations, including
// cross-process ones that swap the main RenderFrameHost. A capture device can
// however be destroyed and re-created *within a single capture session*: when
// a consumer pauses a stream (blink::mojom::MediaStreamStateChange::PAUSE), the
// last capture client disconnects and VideoCaptureManager releases the device
// entirely. Resuming re-creates it, which resolves the captured tab from the
// original routing ID all over again.
//
// If the captured tab performed a cross-process navigation in the meantime --
// which is guaranteed for pause/resume driven by enterprise URL policy, since
// the pause is triggered by navigating to a different site -- that
// RenderFrameHost no longer exists. The lookup then fails and the capture
// target is reported as permanently lost, which aborts the whole stream with a
// fatal error rather than resuming it.
//
// This registry bridges that gap. Entries are weak: once the WebContents is
// gone the lookup correctly fails, and the capture target really is
// permanently lost.
class CapturedTabRegistry {
 public:
  static CapturedTabRegistry& Get() {
    static base::NoDestructor<CapturedTabRegistry> instance;
    return *instance;
  }

  CapturedTabRegistry(const CapturedTabRegistry&) = delete;
  CapturedTabRegistry& operator=(const CapturedTabRegistry&) = delete;

  void Register(const GlobalRenderFrameHostId& id, WebContents* web_contents) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(web_contents);
    // Opportunistically drop entries whose WebContents has gone away, so the
    // map stays proportional to the number of live captured tabs.
    std::erase_if(entries_, [](const auto& entry) { return !entry.second; });
    entries_[id] = web_contents->GetWeakPtr();
  }

  WebContents* Lookup(const GlobalRenderFrameHostId& id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
      return nullptr;
    }
    if (!it->second) {
      entries_.erase(it);
      return nullptr;
    }
    return it->second.get();
  }

 private:
  friend class base::NoDestructor<CapturedTabRegistry>;

  CapturedTabRegistry() = default;
  ~CapturedTabRegistry() = default;

  std::map<GlobalRenderFrameHostId, base::WeakPtr<WebContents>> entries_;
};

}  // namespace

// Note on lifetime: this context is deleted via WebContentsObserver's
// WebContentsDestroyed() method when the WebContents is destroyed.
class WebContentsContext : public WebContentsFrameTracker::Context {
 public:
  explicit WebContentsContext(WebContents* contents) : contents_(contents) {
    CHECK(contents_, base::NotFatalUntil::M160);
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

 private:
  // WebContentsAutoScaler::Delegate:
  void SetCaptureScaleOverride(float scale) override {
    if (auto* view = GetCurrentView()) {
      view->SetScaleOverrideForCapture(scale);
    }
  }

  float GetCaptureScaleOverride() const override {
    if (const auto* view = GetCurrentView()) {
      return view->GetScaleOverrideForCapture();
    }
    // Otherwise we can assume it's unset and return the default value.
    return 1.0f;
  }

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

WebContentsFrameTracker::WebContentsFrameTracker(
    scoped_refptr<base::SequencedTaskRunner> device_task_runner,
    base::WeakPtr<WebContentsVideoCaptureDevice> device,
    MouseCursorOverlayController* cursor_controller)
    : device_(std::move(device)),
      device_task_runner_(std::move(device_task_runner))
#if !BUILDFLAG(IS_IOS)
      ,
      cursor_controller_(cursor_controller->GetWeakPtr())
#endif
{
  // Verify on construction that this object is created on the UI thread.  After
  // this, depend on the sequence checker to ensure consistent execution.
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M160);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(device_task_runner_);

#if !BUILDFLAG(IS_IOS)
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
  CHECK(!is_capturing_, base::NotFatalUntil::M160);
  if (!web_contents()) {
    return;
  }

  capture_size_ = capture_size;
  if (is_high_dpi_enabled &&
      !GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled()) {
    auto_scaler_ =
        std::make_unique<WebContentsAutoScaler>(*context_, capture_size);
  }
  context_->IncrementCapturerCount(CalculatePreferredSize(capture_size));
  is_capturing_ = true;
}

void WebContentsFrameTracker::DidStopCapturingWebContents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents()) {
    CHECK(is_capturing_, base::NotFatalUntil::M160);
    context_->DecrementCapturerCount();
    is_capturing_ = false;

    if (auto_scaler_) {
      UMA_HISTOGRAM_COUNTS_1000("Media.VideoCapture.ScaleOverrideChangeCount",
                                auto_scaler_->GetScaleOverrideChangeCount());
      context_->SetCaptureScaleOverride(1.0f);
      auto_scaler_.reset();
    }
  }
  CHECK(!is_capturing_, base::NotFatalUntil::M160);
}

void WebContentsFrameTracker::SetCapturedContentSize(
    const gfx::Size& content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!web_contents()) {
    return;
  }

  // For efficiency, this function should only be called when the captured
  // content size changes. The caller is responsible for enforcing that.
  TRACE_EVENT_INSTANT("gpu.capture",
                      "WebContentsFrameTracker::SetCapturedContentSize",
                      "content_size", content_size.ToString());

  if (auto_scaler_) {
    auto_scaler_->SetCapturedContentSize(content_size);
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

void WebContentsFrameTracker::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  TRACE_EVENT_INSTANT("gpu.capture",
                      "WebContentsFrameTracker::OnUtilizationReport",
                      "utilization", feedback.resource_utilization,
                      "max_pixels", feedback.max_pixels);
  if (auto_scaler_) {
    auto_scaler_->OnUtilizationReport(std::move(feedback));
  }
}

void WebContentsFrameTracker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
  if (auto_scaler_) {
    SetScaleOverrideForCapture(render_frame_host,
                               auto_scaler_->GetDesiredCaptureScaleOverride());
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
  SetScaleOverrideForCapture(old_host, 1.0f);
  if (auto_scaler_) {
    SetScaleOverrideForCapture(new_host,
                               auto_scaler_->GetDesiredCaptureScaleOverride());
  }
}

void WebContentsFrameTracker::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_capturing_ = false;
  auto_scaler_.reset();
  context_.reset();
  Observe(nullptr);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::OnVisibilityChanged(Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::CaptureTargetChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPossibleTargetChange();
}

void WebContentsFrameTracker::SetWebContentsAndContextFromRoutingId(
    const GlobalRenderFrameHostId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WebContents* target =
      WebContents::FromRenderFrameHost(RenderFrameHost::FromID(id));
  if (target) {
    // Remember the association, so that a capture device re-created later in
    // the same capture session can still find this tab even if `id` has gone
    // away in the meantime due to a cross-process navigation.
    CapturedTabRegistry::Get().Register(id, target);
  } else {
    // The main RenderFrameHost that `id` refers to is gone. This does not mean
    // the captured tab is gone: a cross-process navigation swaps the main
    // RenderFrameHost while the tab itself lives on, and capture is expected to
    // follow the tab. Recover the tab if it is still around.
    target = CapturedTabRegistry::Get().Lookup(id);
  }

  Observe(target);
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
  CHECK(callback, base::NotFatalUntil::M160);

  if (sub_capture_target_version_ >= sub_capture_target_version) {
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
    CHECK(!context_, base::NotFatalUntil::M160);
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
  //
  // We only want to do a mouse overlay if this not a background tab, so
  // we disregard the view if it is. This is only needed if multiple tabs
  // share the same native view (which happens with SurfaceEmbed).
  SetTargetView(web_contents()->GetVisibility() != Visibility::HIDDEN
                    ? capture_target.view
                    : gfx::NativeView());
}

void WebContentsFrameTracker::SetTargetView(gfx::NativeView view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't have to worry about web_contents() changing other than to null
  // since SetWebContentsAndContextFromRoutingId() is only called once.
  if (view == target_native_view_) {
    return;
  }
  target_native_view_ = view;
#if !BUILDFLAG(IS_IOS)
  if (cursor_controller_) {
    cursor_controller_->SetTargetView(view, web_contents());
  }
#endif
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
