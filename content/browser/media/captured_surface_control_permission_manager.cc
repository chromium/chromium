// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_control_permission_manager.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

namespace {

using ::blink::mojom::PermissionStatus;
using PermissionResult =
    ::content::CapturedSurfaceControlPermissionManager::PermissionResult;

// Checks whether the app is focused.
// Note that this is different from requiring that the capturer RFH is focused.
// The check here starts at the primary main frame, and then cascades through
// the tree - which is the desired behavior.
bool IsFocused(WebContentsImpl& web_contents) {
  RenderFrameHostImpl* const rfhi = web_contents.GetPrimaryMainFrame();
  return rfhi && rfhi->IsFocused();
}

// Translate between callbacks expecting different types.
base::OnceCallback<void(PermissionStatus)> WrapCallback(
    base::OnceCallback<void(PermissionResult)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(PermissionResult)> callback,
         PermissionStatus permission_status) {
        std::move(callback).Run(permission_status == PermissionStatus::GRANTED
                                    ? PermissionResult::kGranted
                                    : PermissionResult::kDenied);
      },
      std::move(callback));
}

void CheckPermissionOnUIThread(
    bool sticky_permissions,
    GlobalRenderFrameHostId capturer_rfh_id,
    base::OnceCallback<void(PermissionResult)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* const capturer_rfhi =
      RenderFrameHostImpl::FromID(capturer_rfh_id);
  if (!capturer_rfhi) {
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  BrowserContext* const browser_context = capturer_rfhi->GetBrowserContext();
  if (!browser_context) {
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  WebContentsImpl* const capturer_wc =
      WebContentsImpl::FromRenderFrameHostImpl(capturer_rfhi);
  if (!capturer_wc) {
    // The capturing frame or tab appears to have closed asynchronously.
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  if (!IsFocused(*capturer_wc)) {
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  PermissionController* const permission_controller =
      browser_context->GetPermissionController();
  if (!permission_controller) {
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  if (sticky_permissions) {
    switch (permission_controller->GetPermissionStatusForCurrentDocument(
        blink::PermissionType::CAPTURED_SURFACE_CONTROL, capturer_rfhi)) {
      case PermissionStatus::GRANTED:
        std::move(callback).Run(PermissionResult::kGranted);
        return;
      case PermissionStatus::DENIED:
        std::move(callback).Run(PermissionResult::kDenied);
        return;
      case PermissionStatus::ASK:
        break;
    }
  }

  const bool user_gesture = capturer_rfhi->HasTransientUserActivation();
  if (!user_gesture) {
    std::move(callback).Run(PermissionResult::kDenied);
    return;
  }

  permission_controller->RequestPermissionFromCurrentDocument(
      capturer_rfhi,
      PermissionRequestDescription(
          blink::PermissionType::CAPTURED_SURFACE_CONTROL, user_gesture),
      WrapCallback(std::move(callback)));
}

}  // namespace

CapturedSurfaceControlPermissionManager::
    CapturedSurfaceControlPermissionManager(
        GlobalRenderFrameHostId capturer_rfh_id)
    : capturer_rfh_id_(capturer_rfh_id),
      sticky_permissions_(base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlStickyPermissions)),
      granted_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutoGrantCapturedSurfaceControlPrompt)) {}

CapturedSurfaceControlPermissionManager::
    ~CapturedSurfaceControlPermissionManager() = default;

void CapturedSurfaceControlPermissionManager::CheckPermission(
    base::OnceCallback<void(PermissionResult)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (granted_) {
    std::move(callback).Run(PermissionResult::kGranted);
    return;
  }

  if (!sticky_permissions_) {
    if (has_pending_prompt_ || attempts_left_until_embargo_ <= 0) {
      std::move(callback).Run(PermissionResult::kDenied);
      return;
    }

    has_pending_prompt_ = true;
    --attempts_left_until_embargo_;
  }

  // After CheckPermissionOnUIThread() is done (on the UI thread) it will
  // report back to `this` object (on the IO thread), but only after hopping
  // through a static method that will ensure that the `callback` is invoked
  // even if the capture stops while the prompt is pending.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CheckPermissionOnUIThread, sticky_permissions_, capturer_rfh_id_,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &CapturedSurfaceControlPermissionManager::OnCheckResultStatic,
              weak_factory_.GetWeakPtr(), std::move(callback)))));
}

// static
void CapturedSurfaceControlPermissionManager::OnCheckResultStatic(
    base::WeakPtr<CapturedSurfaceControlPermissionManager> manager,
    base::OnceCallback<void(PermissionResult)> callback,
    PermissionResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!manager) {
    // Intentionally ignore `result`, as the capture-session stopped
    // asynchronously and the result is no longer relevant.
    std::move(callback).Run(PermissionResult::kError);
    return;
  }

  manager->OnCheckResult(std::move(callback), result);
}

void CapturedSurfaceControlPermissionManager::OnCheckResult(
    base::OnceCallback<void(PermissionResult)> callback,
    PermissionResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!sticky_permissions_) {
    has_pending_prompt_ = false;
    granted_ = (result == PermissionResult::kGranted);
  }

  std::move(callback).Run(result);
}

}  // namespace content
