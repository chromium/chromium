// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/captured_surface_controller.h"

#include "base/task/bind_post_task.h"
#include "content/browser/renderer_host/media/captured_surface_control_permission_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

namespace {

using ::blink::mojom::CapturedSurfaceControlResult;
using PermissionManager = ::content::CapturedSurfaceControlPermissionManager;
using PermissionResult =
    ::content::CapturedSurfaceControlPermissionManager::PermissionResult;

// Deliver a synthetic MouseWheel action on the tab whose ID is
// `wc_id`, with the parameters described by the values in `action`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
//
// This function must be invoked on the UI thread with a non-null
// `WebContentsMediaCaptureId`. Note however that the WebContents in question
// might have been asynchronously destroyed in the intervening time, which is
// one possible reason for failure.
CapturedSurfaceControlResult DoSendWheel(
    WebContentsMediaCaptureId wc_id,
    blink::mojom::CapturedWheelActionPtr action) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!wc_id.is_null());

  RenderFrameHost* const rfh = RenderFrameHost::FromID(
      wc_id.render_process_id, wc_id.main_render_frame_id);
  if (!rfh) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  // TODO(crbug.com/1466247): Implement.
  return CapturedSurfaceControlResult::kSuccess;
}

// Set the zoom level of the tab indicated by `wc_id` to `zoom_level`.
//
// Return `CapturedSurfaceControlResult` to be reported back to the renderer,
// indicating success or failure (with reason).
//
// This function must be invoked on the UI thread with a non-null
// `WebContentsMediaCaptureId`. Note however that the WebContents in question
// might have been asynchronously destroyed in the intervening time, which is
// one possible reason for failure.
CapturedSurfaceControlResult DoSetZoomLevel(WebContentsMediaCaptureId wc_id,
                                            int zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!wc_id.is_null());

  RenderFrameHost* const rfh = RenderFrameHost::FromID(
      wc_id.render_process_id, wc_id.main_render_frame_id);
  if (!rfh) {
    return CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError;
  }

  // TODO(crbug.com/1466247): Implement.
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
      base::BindPostTask(content::GetIOThreadTaskRunner({}),
                         std::move(reply_callback));

  return base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&OnPermissionCheckResult, std::move(action_callback),
                     std::move(reply_callback_io)));
}

}  // namespace

CapturedSurfaceController::CapturedSurfaceController(
    GlobalRenderFrameHostId capturer_rfh_id,
    WebContentsMediaCaptureId captured_wc_id)
    : CapturedSurfaceController(
          captured_wc_id,
          std::make_unique<PermissionManager>(capturer_rfh_id)) {}

CapturedSurfaceController::CapturedSurfaceController(
    WebContentsMediaCaptureId captured_wc_id,
    std::unique_ptr<PermissionManager> permission_manager)
    : captured_wc_id_(captured_wc_id),
      permission_manager_(std::move(permission_manager)) {}

CapturedSurfaceController::~CapturedSurfaceController() = default;

void CapturedSurfaceController::UpdateCaptureTarget(
    WebContentsMediaCaptureId captured_wc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  captured_wc_id_ = captured_wc_id;
}

void CapturedSurfaceController::SendWheel(
    blink::mojom::CapturedWheelActionPtr action,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (captured_wc_id_.is_null()) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  // Action to be performed on the UI thread if permitted.
  base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback =
      base::BindOnce(&DoSendWheel, captured_wc_id_, std::move(action));

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

void CapturedSurfaceController::SetZoomLevel(
    int zoom_level,
    base::OnceCallback<void(CapturedSurfaceControlResult)> reply_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (captured_wc_id_.is_null()) {
    std::move(reply_callback)
        .Run(CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
    return;
  }

  // Action to be performed on the UI thread if permitted.
  base::OnceCallback<CapturedSurfaceControlResult(void)> action_callback =
      base::BindOnce(&DoSetZoomLevel, captured_wc_id_, zoom_level);

  permission_manager_->CheckPermission(
      ComposeCallbacks(std::move(action_callback), std::move(reply_callback)));
}

}  // namespace content
