// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_controller.h"

#include <cmath>

#include "base/task/bind_post_task.h"
#include "content/browser/media/media_stream_web_contents_observer.h"
#include "content/browser/media/captured_surface_control_permission_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
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
using PermissionManager = ::content::CapturedSurfaceControlPermissionManager;
using PermissionResult =
    ::content::CapturedSurfaceControlPermissionManager::PermissionResult;
using GetZoomLevelReplyCallback =
    base::OnceCallback<void(std::optional<int> zoom_level,
                            blink::mojom::CapturedSurfaceControlResult result)>;

base::WeakPtr<WebContents> ResolveWebContentsOnUI(
    WebContentsMediaCaptureId wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (wc_id.is_null()) {
    return nullptr;
  }

  WebContents* const wc =
      WebContents::FromRenderFrameHost(RenderFrameHost::FromID(
          wc_id.render_process_id, wc_id.main_render_frame_id));

  return wc ? wc->GetWeakPtr() : nullptr;
}

// Deliver a synthetic MouseWheel action on `captured_wc` with the parameters
// described by the values in `action`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
CapturedSurfaceControlResult DoSendWheel(
    base::WeakPtr<WebContents> captured_wc,
    blink::mojom::CapturedWheelActionPtr action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* const rfh =
      captured_wc ? captured_wc->GetPrimaryMainFrame() : nullptr;
  if (!rfh) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  RenderFrameHostImpl* const rfhi =
      RenderFrameHostImpl::FromID(rfh->GetGlobalId());
  RenderWidgetHostImpl* const rwhi =
      rfhi ? rfhi->GetRenderWidgetHost() : nullptr;
  if (!rwhi) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  // Scale (x, y).
  const gfx::Size captured_viewport_size = rwhi->GetRootWidgetViewportSize();
  if (captured_viewport_size.width() < 1 ||
      captured_viewport_size.height() < 1) {
    return CapturedSurfaceControlResult::kUnknownError;
  }
  const double x =
      std::floor(action->relative_x * captured_viewport_size.width());
  const double y =
      std::floor(action->relative_y * captured_viewport_size.height());

  // Produce the wheel event on the captured surface.
  {
    blink::WebMouseWheelEvent event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, action->wheel_delta_x, action->wheel_delta_y,
            blink::WebInputEvent::kNoModifiers,
            ui::ScrollGranularity::kScrollByPixel);
    event.phase = blink::WebMouseWheelEvent::Phase::kPhaseBegan;
    rwhi->ForwardWheelEvent(event);
  }

  // Close the loop by producing an event at the same location with zero deltas
  // and with the phase set to kPhaseEnded.
  {
    blink::WebMouseWheelEvent event =
        blink::SyntheticWebMouseWheelEventBuilder::Build(
            x, y, /*dx=*/0, /*dy=*/0, blink::WebInputEvent::kNoModifiers,
            ui::ScrollGranularity::kScrollByPixel);
    event.phase = blink::WebMouseWheelEvent::Phase::kPhaseEnded;
    rwhi->ForwardWheelEvent(event);
  }

  return CapturedSurfaceControlResult::kSuccess;
}

// Set the zoom level of the tab indicated by `captured_wc` to `zoom_level`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
CapturedSurfaceControlResult DoSetZoomLevel(
    base::WeakPtr<WebContents> captured_wc,
    int zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!captured_wc) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  content::HostZoomMap::SetZoomLevel(
      captured_wc.get(),
      blink::PageZoomFactorToZoomLevel(static_cast<double>(zoom_level) / 100));
  return CapturedSurfaceControlResult::kSuccess;
}

// Get the zoom level of the tab indicated by `captured_wc`.
//
// Return the zoom_level if successful or nullopt otherwise.
std::pair<std::optional<int>, CapturedSurfaceControlResult> DoGetZoomLevel(
    base::WeakPtr<WebContents> captured_wc) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!captured_wc) {
    return std::make_pair(
        std::nullopt,
        CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
  }

  double zoom_level = blink::PageZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(captured_wc.get()));
  return std::make_pair(std::round(100 * zoom_level),
                        CapturedSurfaceControlResult::kSuccess);
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
      base::BindPostTask(content::GetIOThreadTaskRunner({}),
                         std::move(reply_callback));

  return base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&OnPermissionCheckResult, std::move(action_callback),
                     std::move(reply_callback_io)));
}

}  // namespace

std::unique_ptr<CapturedSurfaceController>
CapturedSurfaceController::CreateForTesting(
    WebContentsMediaCaptureId captured_wc_id,
    std::unique_ptr<CapturedSurfaceControlPermissionManager> permission_manager,
    base::RepeatingCallback<void()> wc_resolution_callback) {
  return base::WrapUnique(new CapturedSurfaceController(
      captured_wc_id, std::move(permission_manager),
      std::move(wc_resolution_callback)));
}

CapturedSurfaceController::CapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id)
    : CapturedSurfaceController(
          captured_wc_id,
          std::make_unique<PermissionManager>(capturer_rfh_id),
          /*wc_resolution_callback=*/base::DoNothing()) {}

CapturedSurfaceController::CapturedSurfaceController(
    WebContentsMediaCaptureId captured_wc_id,
    std::unique_ptr<PermissionManager> permission_manager,
    base::RepeatingCallback<void()> wc_resolution_callback)
    : permission_manager_(std::move(permission_manager)),
      wc_resolution_callback_(std::move(wc_resolution_callback)) {
  ResolveCapturedWebContents(captured_wc_id);
}

CapturedSurfaceController::~CapturedSurfaceController() = default;

void CapturedSurfaceController::UpdateCaptureTarget(
    WebContentsMediaCaptureId captured_wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResolveCapturedWebContents(captured_wc_id);
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
      base::BindOnce(&DoSendWheel, captured_wc_.value(), std::move(action));

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::GetZoomLevel(
    GetZoomLevelReplyCallback reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!captured_wc_.has_value()) {
    std::move(reply_callback)
        .Run(std::nullopt,
             CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetZoomLevel, captured_wc_.value()),
      base::BindOnce(
          [](GetZoomLevelReplyCallback reply_callback,
             std::pair<std::optional<int>, CapturedSurfaceControlResult>
                 result) {
            std::move(reply_callback).Run(result.first, result.second);
          },
          std::move(reply_callback)));
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
      base::BindOnce(&DoSetZoomLevel, captured_wc_.value(), zoom_level);

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::ResolveCapturedWebContents(
    WebContentsMediaCaptureId captured_wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Avoid posting new tasks (DoSendWheel/DoSetZoomLevel) with the old target
  // while pending resolution.
  captured_wc_ = absl::nullopt;

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ResolveWebContentsOnUI, captured_wc_id),
      base::BindOnce(&CapturedSurfaceController::OnCapturedWebContentsResolved,
                     weak_factory_.GetWeakPtr()));
}

void CapturedSurfaceController::OnCapturedWebContentsResolved(
    base::WeakPtr<WebContents> captured_wc) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Note that `captured_wc_` may be non-nullopt if a second call to
  // ResolveWebContentsOnUI() was scheduled before the first one resolved.
  captured_wc_ = captured_wc;
  wc_resolution_callback_.Run();
}

}  // namespace content
