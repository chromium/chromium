// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_provider.h"

#include <utility>

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/web_contents.h"

namespace content {

using device::mojom::ScreenOrientationLockResult;

ScreenOrientationDelegate* ScreenOrientationProvider::delegate_ = nullptr;

ScreenOrientationProvider::ScreenOrientationProvider(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      lock_applied_(false),
      receivers_(web_contents, this) {}

ScreenOrientationProvider::~ScreenOrientationProvider() = default;

void ScreenOrientationProvider::BindScreenOrientation(
    RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>
        receiver) {
  receivers_.Bind(rfh, std::move(receiver));
}

void ScreenOrientationProvider::LockOrientation(
    device::mojom::ScreenOrientationLockType orientation,
    LockOrientationCallback callback) {
  // Cancel any pending lock request.
  NotifyLockResult(ScreenOrientationLockResult::
                       SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
  // Record new pending lock request.
  pending_callback_ = std::move(callback);

  if (!delegate_ ||
      !delegate_->ScreenOrientationProviderSupported(web_contents())) {
    NotifyLockResult(ScreenOrientationLockResult::
                         SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE);
    return;
  }

  if (delegate_->FullScreenRequired(web_contents())) {
    RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
        web_contents()->GetPrimaryMainFrame()->GetRenderViewHost());
    if (!rvhi) {
      NotifyLockResult(ScreenOrientationLockResult::
                           SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
      return;
    }
    if (!static_cast<WebContentsImpl*>(web_contents())->IsFullscreen() &&
        static_cast<WebContentsImpl*>(web_contents())->GetDisplayMode() !=
            blink::mojom::DisplayMode::kFullscreen) {
      NotifyLockResult(
          ScreenOrientationLockResult::
              SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED);
      return;
    }
  }

  if (orientation == device::mojom::ScreenOrientationLockType::NATURAL) {
    orientation = GetNaturalLockType();
    if (orientation == device::mojom::ScreenOrientationLockType::DEFAULT) {
      // We are in a broken state, let's pretend we got canceled.
      NotifyLockResult(ScreenOrientationLockResult::
                           SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);
      return;
    }
  }

  lock_applied_ = true;
  delegate_->Lock(web_contents(), orientation);
  if (auto* view = web_contents()->GetRenderWidgetHostView())
    static_cast<RenderWidgetHostViewBase*>(view)->LockOrientation(orientation);

  // If the orientation we are locking to matches the current orientation, we
  // should succeed immediately.
  if (LockMatchesCurrentOrientation(orientation)) {
    NotifyLockResult(
        ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
    return;
  }

  pending_lock_orientation_ = orientation;
}

void ScreenOrientationProvider::UnlockOrientation() {
  // Cancel any pending lock request.
  NotifyLockResult(ScreenOrientationLockResult::
                       SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED);

  if (!lock_applied_ || !delegate_)
    return;

  delegate_->Unlock(web_contents());
  if (auto* view = web_contents()->GetRenderWidgetHostView())
    static_cast<RenderWidgetHostViewBase*>(view)->UnlockOrientation();

  lock_applied_ = false;
}

void ScreenOrientationProvider::OnOrientationChange() {
  if (!pending_lock_orientation_.has_value())
    return;

  if (LockMatchesCurrentOrientation(pending_lock_orientation_.value())) {
    DCHECK(!pending_callback_.is_null());
    NotifyLockResult(
        ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS);
  }
}

void ScreenOrientationProvider::NotifyLockResult(
    ScreenOrientationLockResult result) {
  if (!pending_callback_.is_null())
    std::move(pending_callback_).Run(result);

  pending_lock_orientation_.reset();
}

void ScreenOrientationProvider::SetDelegate(
    ScreenOrientationDelegate* delegate) {
  delegate_ = delegate;
}

ScreenOrientationDelegate* ScreenOrientationProvider::GetDelegateForTesting() {
  return delegate_;
}

bool ScreenOrientationProvider::LockMatchesOrientation(
    device::mojom::ScreenOrientationLockType lock,
    display::mojom::ScreenOrientation orientation) {
  switch (lock) {
    case device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY:
      return orientation == display::mojom::ScreenOrientation::kPortraitPrimary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT_SECONDARY:
      return orientation ==
             display::mojom::ScreenOrientation::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY:
      return orientation ==
             display::mojom::ScreenOrientation::kLandscapePrimary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE_SECONDARY:
      return orientation ==
             display::mojom::ScreenOrientation::kLandscapeSecondary;
    case device::mojom::ScreenOrientationLockType::LANDSCAPE:
      return orientation ==
                 display::mojom::ScreenOrientation::kLandscapePrimary ||
             orientation ==
                 display::mojom::ScreenOrientation::kLandscapeSecondary;
    case device::mojom::ScreenOrientationLockType::PORTRAIT:
      return orientation ==
                 display::mojom::ScreenOrientation::kPortraitPrimary ||
             orientation ==
                 display::mojom::ScreenOrientation::kPortraitSecondary;
    case device::mojom::ScreenOrientationLockType::ANY:
      return true;
    case device::mojom::ScreenOrientationLockType::NATURAL:
    case device::mojom::ScreenOrientationLockType::DEFAULT:
      return false;
  }

  return false;
}

void ScreenOrientationProvider::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  if (!lock_applied_ || !delegate_)
    return;

  // If fullscreen is not required in order to lock orientation, don't unlock
  // when fullscreen state changes.
  if (!delegate_->FullScreenRequired(web_contents()))
    return;

  DCHECK(!entered_fullscreen);
  UnlockOrientation();
}

void ScreenOrientationProvider::PrimaryPageChanged(Page& page) {
  UnlockOrientation();
}

device::mojom::ScreenOrientationLockType
ScreenOrientationProvider::GetNaturalLockType() const {
  RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return device::mojom::ScreenOrientationLockType::DEFAULT;

  display::ScreenInfo screen_info = rwh->GetScreenInfo();

  switch (screen_info.orientation_type) {
    case display::mojom::ScreenOrientation::kPortraitPrimary:
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
      }
      return device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return device::mojom::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
      }
      return device::mojom::ScreenOrientationLockType::PORTRAIT_PRIMARY;
    default:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return device::mojom::ScreenOrientationLockType::DEFAULT;
}

bool ScreenOrientationProvider::LockMatchesCurrentOrientation(
    device::mojom::ScreenOrientationLockType lock) {
  RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return false;

  display::ScreenInfo screen_info = rwh->GetScreenInfo();

  if (lock == device::mojom::ScreenOrientationLockType::NATURAL ||
      lock == device::mojom::ScreenOrientationLockType::DEFAULT) {
    NOTREACHED_IN_MIGRATION();
  }
  return LockMatchesOrientation(lock, screen_info.orientation_type);
}

}  // namespace content
