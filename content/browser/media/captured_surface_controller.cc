// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_controller.h"

#include <cmath>

#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "content/browser/media/media_stream_web_contents_observer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

using ::blink::mojom::CapturedSurfaceControlResult;
using PermissionManager = CapturedSurfaceControlPermissionManager;
using PermissionResult = PermissionManager::PermissionResult;
using GetZoomLevelReplyCallback =
    base::OnceCallback<void(std::optional<int> zoom_level,
                            blink::mojom::CapturedSurfaceControlResult result)>;
using CapturedSurfaceInfo = CapturedSurfaceController::CapturedSurfaceInfo;

void OnZoomLevelChangeOnUI(
    base::RepeatingCallback<void(int)> on_zoom_level_change_callback,
    base::WeakPtr<WebContents> captured_wc,
    const HostZoomMap::ZoomLevelChange& change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!captured_wc) {
    return;
  }

  int zoom_level =
      std::round(100 * blink::ZoomLevelToZoomFactor(
                           HostZoomMap::GetZoomLevel(captured_wc.get())));
  on_zoom_level_change_callback.Run(zoom_level);
}

std::optional<CapturedSurfaceInfo> ResolveCapturedSurfaceOnUI(
    WebContentsMediaCaptureId wc_id,
    int subscription_version,
    base::RepeatingCallback<void(int)> on_zoom_level_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (wc_id.is_null()) {
    return std::nullopt;
  }

  WebContents* const wc =
      WebContents::FromRenderFrameHost(RenderFrameHost::FromID(
          wc_id.render_process_id, wc_id.main_render_frame_id));

  if (!wc) {
    return std::nullopt;
  }

  HostZoomMap* host_zoom_map = HostZoomMap::GetForWebContents(wc);
  if (!host_zoom_map) {
    return std::nullopt;
  }

  int initial_zoom_level = std::round(
      100 * blink::ZoomLevelToZoomFactor(HostZoomMap::GetZoomLevel(wc)));

  std::unique_ptr<base::CallbackListSubscription,
                  BrowserThread::DeleteOnUIThread>
      subscription_ptr(new base::CallbackListSubscription());
  base::WeakPtr<WebContents> wc_weak_ptr = wc->GetWeakPtr();
  *subscription_ptr =
      host_zoom_map->AddZoomLevelChangedCallback(base::BindRepeating(
          &OnZoomLevelChangeOnUI, on_zoom_level_change_callback, wc_weak_ptr));

  return CapturedSurfaceInfo(wc_weak_ptr, std::move(subscription_ptr),
                             subscription_version, initial_zoom_level);
}

// Deliver a synthetic MouseWheel action on `captured_wc` with the parameters
// described by the values in `action`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
CapturedSurfaceControlResult DoSendWheel(
    GlobalRenderFrameHostId capturer_rfh_id,
    base::WeakPtr<WebContents> captured_wc,
    blink::mojom::CapturedWheelActionPtr action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* const capturer_wci =
      WebContentsImpl::FromRenderFrameHostImpl(
          RenderFrameHostImpl::FromID(capturer_rfh_id));
  if (!capturer_wci) {
    // The capturing frame or tab appears to have closed asynchronously.
    return CapturedSurfaceControlResult::kCapturerNotFoundError;
  }

  RenderFrameHost* const captured_rfh =
      captured_wc ? captured_wc->GetPrimaryMainFrame() : nullptr;
  if (!captured_rfh) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  RenderFrameHostImpl* const captured_rfhi =
      RenderFrameHostImpl::FromID(captured_rfh->GetGlobalId());
  RenderWidgetHostImpl* const captured_rwhi =
      captured_rfhi ? captured_rfhi->GetRenderWidgetHost() : nullptr;
  if (!captured_rwhi) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  if (capturer_wci == captured_wc.get()) {
    return CapturedSurfaceControlResult::kDisallowedForSelfCaptureError;
  }

  // Scale (x, y).
  const gfx::Size captured_viewport_size =
      captured_rwhi->GetRenderInputRouter()->GetRootWidgetViewportSize();
  if (captured_viewport_size.width() < 1 ||
      captured_viewport_size.height() < 1) {
    return CapturedSurfaceControlResult::kUnknownError;
  }
  const double x =
      std::floor(action->relative_x * captured_viewport_size.width());
  const double y =
      std::floor(action->relative_y * captured_viewport_size.height());

  // Clamp deltas.
  // Note that `action->wheel_delta_x` and `action->wheel_delta_y` are
  // `int32_t`s, but `blink::SyntheticWebMouseWheelEventBuilder::Build()`
  // receives `float`s.
  const float wheel_delta_x =
      std::min(CapturedSurfaceController::kMaxWheelDeltaMagnitude,
               std::max(action->wheel_delta_x,
                        -CapturedSurfaceController::kMaxWheelDeltaMagnitude));
  const float wheel_delta_y =
      std::min(CapturedSurfaceController::kMaxWheelDeltaMagnitude,
               std::max(action->wheel_delta_y,
                        -CapturedSurfaceController::kMaxWheelDeltaMagnitude));

  // Produce the wheel event on the captured surface.
  {
    blink::WebMouseWheelEvent event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, wheel_delta_x, wheel_delta_y,
            blink::WebInputEvent::kNoModifiers,
            ui::ScrollGranularity::kScrollByPixel);
    event.phase = blink::WebMouseWheelEvent::Phase::kPhaseBegan;
    captured_rwhi->ForwardWheelEvent(event);
  }

  // Close the loop by producing an event at the same location with zero deltas
  // and with the phase set to kPhaseEnded.
  {
    blink::WebMouseWheelEvent event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, /*dx=*/0, /*dy=*/0, blink::WebInputEvent::kNoModifiers,
            ui::ScrollGranularity::kScrollByPixel);
    event.phase = blink::WebMouseWheelEvent::Phase::kPhaseEnded;
    captured_rwhi->ForwardWheelEvent(event);
  }

  capturer_wci->DidCapturedSurfaceControl();

  return CapturedSurfaceControlResult::kSuccess;
}

// Set the zoom level of the tab indicated by `captured_wc` to `zoom_level`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
CapturedSurfaceControlResult DoSetZoomLevel(
    GlobalRenderFrameHostId capturer_rfh_id,
    base::WeakPtr<WebContents> captured_wc,
    int zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* const capturer_wci =
      WebContentsImpl::FromRenderFrameHostImpl(
          RenderFrameHostImpl::FromID(capturer_rfh_id));
  if (!capturer_wci) {
    // The capturing frame or tab appears to have closed asynchronously.
    return CapturedSurfaceControlResult::kCapturerNotFoundError;
  }

  if (!captured_wc) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  if (capturer_wci == captured_wc.get()) {
    return CapturedSurfaceControlResult::kDisallowedForSelfCaptureError;
  }

  // TODO(crbug.com/328589994): Hard-code kCapturedSurfaceControlTemporaryZoom.
  if (!base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlTemporaryZoom)) {
    HostZoomMap::SetZoomLevel(
        captured_wc.get(),
        blink::ZoomFactorToZoomLevel(static_cast<double>(zoom_level) / 100));
    return CapturedSurfaceControlResult::kSuccess;
  }

  HostZoomMap* const zoom_map =
      HostZoomMap::GetForWebContents(captured_wc.get());
  if (!zoom_map) {
    return CapturedSurfaceControlResult::kUnknownError;
  }

  zoom_map->SetTemporaryZoomLevel(
      captured_wc->GetPrimaryMainFrame()->GetGlobalId(),
      blink::ZoomFactorToZoomLevel(static_cast<double>(zoom_level) / 100));

  capturer_wci->DidCapturedSurfaceControl();

  return CapturedSurfaceControlResult::kSuccess;
}

// Return success if all conditions for CSC apply, otherwise fail with the
// appropriate error code.
CapturedSurfaceControlResult FinalizeRequestPermission(
    GlobalRenderFrameHostId capturer_rfh_id,
    base::WeakPtr<WebContents> captured_wc) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* const capturer_wci =
      WebContentsImpl::FromRenderFrameHostImpl(
          RenderFrameHostImpl::FromID(capturer_rfh_id));
  if (!capturer_wci) {
    // The capturing frame or tab appears to have closed asynchronously.
    return CapturedSurfaceControlResult::kCapturerNotFoundError;
  }

  RenderFrameHost* const captured_rfh =
      captured_wc ? captured_wc->GetPrimaryMainFrame() : nullptr;
  if (!captured_rfh) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  RenderFrameHostImpl* const captured_rfhi =
      RenderFrameHostImpl::FromID(captured_rfh->GetGlobalId());
  RenderWidgetHostImpl* const captured_rwhi =
      captured_rfhi ? captured_rfhi->GetRenderWidgetHost() : nullptr;
  if (!captured_rwhi) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  if (capturer_wci == captured_wc.get()) {
    return CapturedSurfaceControlResult::kDisallowedForSelfCaptureError;
  }

  return CapturedSurfaceControlResult::kSuccess;
}

void OnPermissionCheckResult(
    base::OnceCallback<CapturedSurfaceControlResult()> action_callback,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback,
    PermissionResult permission_check_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (permission_check_result == PermissionResult::kDenied) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kNoPermissionError);
    return;
  }

  if (permission_check_result == PermissionResult::kError) {
    std::move(reply_callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  const CapturedSurfaceControlResult result = std::move(action_callback).Run();
  std::move(reply_callback).Run(result);
}

// Given:
// 1. A callback that will attempt to perform an action if permitted.
// 2. A callback that will report to the renderer process whether the
//    action succeeded, failed or was not permitted.
//
// Return:
// A callback that composes these two into a single callback that,
// after the permission manager has checked for permission, runs the
// action callback if it is permitted, and reports the result to the renderer.
//
// It is assumed that `action_callback` runs on the UI thread.
base::OnceCallback<void(PermissionResult)> ComposeCallbacks(
    base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  // Callback for reporting result of both permission-prompt as well as action
  // (if permitted) to the renderer.
  base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback_io =
      base::BindPostTask(GetIOThreadTaskRunner({}), std::move(reply_callback));

  return base::BindPostTask(
      GetUIThreadTaskRunner({}),
      base::BindOnce(&OnPermissionCheckResult, std::move(action_callback),
                     std::move(reply_callback_io)));
}

}  // namespace

CapturedSurfaceInfo::CapturedSurfaceInfo(
    base::WeakPtr<WebContents> captured_wc,
    std::unique_ptr<base::CallbackListSubscription,
                    BrowserThread::DeleteOnUIThread> subscription,
    int subscription_version,
    int initial_zoom_level)
    : captured_wc(captured_wc),
      subscription(std::move(subscription)),
      subscription_version(subscription_version),
      initial_zoom_level(initial_zoom_level) {}

CapturedSurfaceInfo::CapturedSurfaceInfo(CapturedSurfaceInfo&& other) = default;
CapturedSurfaceInfo& CapturedSurfaceInfo::operator=(
    CapturedSurfaceInfo&& other) = default;

CapturedSurfaceInfo::~CapturedSurfaceInfo() = default;

std::unique_ptr<CapturedSurfaceController>
CapturedSurfaceController::CreateForTesting(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id,
    std::unique_ptr<PermissionManager> permission_manager,
    base::RepeatingCallback<void(int)> on_zoom_level_change_callback,
    base::RepeatingCallback<void(base::WeakPtr<WebContents>)>
        wc_resolution_callback) {
  return base::WrapUnique(new CapturedSurfaceController(
      capturer_rfh_id, captured_wc_id, std::move(permission_manager),
      on_zoom_level_change_callback, std::move(wc_resolution_callback)));
}

CapturedSurfaceController::CapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id,
    base::RepeatingCallback<void(int)> on_zoom_level_change_callback)
    : CapturedSurfaceController(
          capturer_rfh_id,
          captured_wc_id,
          std::make_unique<PermissionManager>(capturer_rfh_id),
          std::move(on_zoom_level_change_callback),
          /*wc_resolution_callback=*/base::DoNothing()) {}

CapturedSurfaceController::CapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id,
    std::unique_ptr<PermissionManager> permission_manager,
    base::RepeatingCallback<void(int)> on_zoom_level_change_callback,
    base::RepeatingCallback<void(base::WeakPtr<WebContents>)>
        wc_resolution_callback)
    : capturer_rfh_id_(capturer_rfh_id),
      permission_manager_(std::move(permission_manager)),
      wc_resolution_callback_(std::move(wc_resolution_callback)),
      on_zoom_level_change_callback_(std::move(on_zoom_level_change_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResolveCapturedSurface(captured_wc_id);
}

CapturedSurfaceController::~CapturedSurfaceController() = default;

void CapturedSurfaceController::UpdateCaptureTarget(
    WebContentsMediaCaptureId captured_wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResolveCapturedSurface(captured_wc_id);
}

void CapturedSurfaceController::SendWheel(
    blink::mojom::CapturedWheelActionPtr action,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!captured_wc_.has_value()) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  // Action to be performed on the UI thread if permitted.
  base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback =
      base::BindOnce(&DoSendWheel, capturer_rfh_id_, captured_wc_.value(),
                     std::move(action));

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::SetZoomLevel(
    int zoom_level,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!captured_wc_.has_value()) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  // Action to be performed on the UI thread if permitted.
  base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback =
      base::BindOnce(&DoSetZoomLevel, capturer_rfh_id_, captured_wc_.value(),
                     zoom_level);

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::RequestPermission(
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!captured_wc_.has_value()) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  // If the permission check is successful, just return success.
  base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback =
      base::BindOnce(&FinalizeRequestPermission, capturer_rfh_id_,
                     captured_wc_.value());
  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::ResolveCapturedSurface(
    WebContentsMediaCaptureId captured_wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Avoid posting new tasks (DoSendWheel/DoSetZoomLevel) with the old target
  // while pending resolution.
  captured_wc_ = std::nullopt;
  zoom_level_subscription_.reset();

  // Ensure that, in the unlikely case that multiple resolutions are pending at
  // the same time, only the resolution of the last one will set `captured_wc_`
  // back to a concrete value.
  ++pending_wc_resolutions_;

  base::RepeatingCallback on_zoom_level_change_callback =
      base::BindRepeating(&CapturedSurfaceController::OnZoomLevelChange,
                          weak_factory_.GetWeakPtr(), ++subscription_version_);

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ResolveCapturedSurfaceOnUI, captured_wc_id, subscription_version_,
          base::BindPostTask(GetIOThreadTaskRunner({}),
                             std::move(on_zoom_level_change_callback))),
      base::BindOnce(&CapturedSurfaceController::OnCapturedSurfaceResolved,
                     weak_factory_.GetWeakPtr()));
}

void CapturedSurfaceController::OnCapturedSurfaceResolved(
    std::optional<CapturedSurfaceInfo> captured_surface_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK_GE(pending_wc_resolutions_, 1);
  if (--pending_wc_resolutions_ > 0) {
    return;
  }
  if (!captured_surface_info) {
    return;
  }

  captured_wc_ = captured_surface_info->captured_wc;
  zoom_level_subscription_ = std::move(captured_surface_info->subscription);
  OnZoomLevelChange(captured_surface_info->subscription_version,
                    captured_surface_info->initial_zoom_level);
  wc_resolution_callback_.Run(captured_surface_info->captured_wc);
}

void CapturedSurfaceController::OnZoomLevelChange(
    int zoom_level_subscription_version,
    int zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only propagate zoom-level updates if they are sent with the current
  // zoom-level subscription version.
  if (zoom_level_subscription_version != subscription_version_) {
    return;
  }

  // Do not propagate if the zoom level has not changed.
  if (current_zoom_level_ == zoom_level) {
    return;
  }

  current_zoom_level_ = zoom_level;
  on_zoom_level_change_callback_.Run(zoom_level);
}

}  // namespace content
