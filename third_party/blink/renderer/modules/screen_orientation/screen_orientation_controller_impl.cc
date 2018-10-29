// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller_impl.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/fullscreen/scoped_allow_fullscreen.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_dispatcher.h"
#include "third_party/blink/renderer/platform/layout_test_support.h"

namespace blink {

ScreenOrientationControllerImpl::~ScreenOrientationControllerImpl() = default;

void ScreenOrientationControllerImpl::ProvideTo(LocalFrame& frame) {
  ScreenOrientationController::ProvideTo(
      frame, new ScreenOrientationControllerImpl(frame));
}

ScreenOrientationControllerImpl* ScreenOrientationControllerImpl::From(
    LocalFrame& frame) {
  return static_cast<ScreenOrientationControllerImpl*>(
      ScreenOrientationController::From(frame));
}

ScreenOrientationControllerImpl::ScreenOrientationControllerImpl(
    LocalFrame& frame)
    : ScreenOrientationController(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      PlatformEventController(frame.GetDocument()),
      dispatch_event_timer_(
          frame.GetTaskRunner(TaskType::kMiscPlatformAPI),
          this,
          &ScreenOrientationControllerImpl::DispatchEventTimerFired) {
  AssociatedInterfaceProvider* provider =
      frame.GetRemoteNavigationAssociatedInterfaces();
  if (provider)
    provider->GetInterface(&screen_orientation_service_);
}

// Compute the screen orientation using the orientation angle and the screen
// width / height.
WebScreenOrientationType ScreenOrientationControllerImpl::ComputeOrientation(
    const IntRect& rect,
    uint16_t rotation) {
  // Bypass orientation detection in layout tests to get consistent results.
  // FIXME: The screen dimension should be fixed when running the layout tests
  // to avoid such issues.
  if (LayoutTestSupport::IsRunningLayoutTest())
    return kWebScreenOrientationPortraitPrimary;

  bool is_tall_display = rotation % 180 ? rect.Height() < rect.Width()
                                        : rect.Height() > rect.Width();

  // https://w3c.github.io/screen-orientation/#dfn-current-orientation-angle
  // allows the UA to associate *-primary and *-secondary values at will. Blink
  // arbitrarily chooses rotation 0 to always be portrait-primary or
  // landscape-primary, and portrait-primary + 90 to be landscape-primary, which
  // together fully determine the relationship.
  switch (rotation) {
    case 0:
      return is_tall_display ? kWebScreenOrientationPortraitPrimary
                             : kWebScreenOrientationLandscapePrimary;
    case 90:
      return is_tall_display ? kWebScreenOrientationLandscapePrimary
                             : kWebScreenOrientationPortraitSecondary;
    case 180:
      return is_tall_display ? kWebScreenOrientationPortraitSecondary
                             : kWebScreenOrientationLandscapeSecondary;
    case 270:
      return is_tall_display ? kWebScreenOrientationLandscapeSecondary
                             : kWebScreenOrientationPortraitPrimary;
    default:
      NOTREACHED();
      return kWebScreenOrientationPortraitPrimary;
  }
}

void ScreenOrientationControllerImpl::UpdateOrientation() {
  DCHECK(orientation_);
  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetPage());
  ChromeClient& chrome_client = GetFrame()->GetPage()->GetChromeClient();
  WebScreenInfo screen_info = chrome_client.GetScreenInfo();
  WebScreenOrientationType orientation_type = screen_info.orientation_type;
  if (orientation_type == kWebScreenOrientationUndefined) {
    // The embedder could not provide us with an orientation, deduce it
    // ourselves.
    orientation_type = ComputeOrientation(chrome_client.GetScreenInfo().rect,
                                          screen_info.orientation_angle);
  }
  DCHECK(orientation_type != kWebScreenOrientationUndefined);

  orientation_->SetType(orientation_type);
  orientation_->SetAngle(screen_info.orientation_angle);
}

bool ScreenOrientationControllerImpl::IsActive() const {
  return orientation_ && screen_orientation_service_;
}

bool ScreenOrientationControllerImpl::IsVisible() const {
  return GetPage() && GetPage()->IsPageVisible();
}

bool ScreenOrientationControllerImpl::IsActiveAndVisible() const {
  return IsActive() && IsVisible();
}

void ScreenOrientationControllerImpl::PageVisibilityChanged() {
  NotifyDispatcher();

  if (!IsActiveAndVisible())
    return;

  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetPage());

  // The orientation type and angle are tied in a way that if the angle has
  // changed, the type must have changed.
  unsigned short current_angle = GetFrame()
                                     ->GetPage()
                                     ->GetChromeClient()
                                     .GetScreenInfo()
                                     .orientation_angle;

  // FIXME: sendOrientationChangeEvent() currently send an event all the
  // children of the frame, so it should only be called on the frame on
  // top of the tree. We would need the embedder to call
  // sendOrientationChangeEvent on every WebFrame part of a WebView to be
  // able to remove this.
  if (GetFrame() == GetFrame()->LocalFrameRoot() &&
      orientation_->angle() != current_angle)
    NotifyOrientationChanged();
}

void ScreenOrientationControllerImpl::NotifyOrientationChanged() {
  if (!IsVisible() || !GetFrame())
    return;

  if (IsActive())
    UpdateOrientation();

  // Keep track of the frames that need to be notified before notifying the
  // current frame as it will prevent side effects from the change event
  // handlers.
  HeapVector<Member<LocalFrame>> child_frames;
  for (Frame* child = GetFrame()->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->IsLocalFrame())
      child_frames.push_back(ToLocalFrame(child));
  }

  // Notify current orientation object.
  if (IsActive() && !dispatch_event_timer_.IsActive())
    dispatch_event_timer_.StartOneShot(TimeDelta(), FROM_HERE);

  // ... and child frames, if they have a ScreenOrientationControllerImpl.
  for (LocalFrame* child_frame : child_frames) {
    if (ScreenOrientationControllerImpl* controller =
            ScreenOrientationControllerImpl::From(*child_frame)) {
      controller->NotifyOrientationChanged();
    }
  }
}

void ScreenOrientationControllerImpl::SetOrientation(
    ScreenOrientation* orientation) {
  orientation_ = orientation;
  if (orientation_)
    UpdateOrientation();
  NotifyDispatcher();
}

void ScreenOrientationControllerImpl::lock(
    WebScreenOrientationLockType orientation,
    std::unique_ptr<WebLockOrientationCallback> callback) {
  // When detached, the |screen_orientation_service_| is no longer valid.
  if (!screen_orientation_service_)
    return;

  CancelPendingLocks();
  pending_callback_ = std::move(callback);
  screen_orientation_service_->LockOrientation(
      orientation,
      WTF::Bind(&ScreenOrientationControllerImpl::OnLockOrientationResult,
                WrapWeakPersistent(this), ++request_id_));

  active_lock_ = true;
}

void ScreenOrientationControllerImpl::unlock() {
  // When detached, the |screen_orientation_service_| is no longer valid.
  if (!screen_orientation_service_)
    return;

  CancelPendingLocks();
  screen_orientation_service_->UnlockOrientation();
  active_lock_ = false;
}

bool ScreenOrientationControllerImpl::MaybeHasActiveLock() const {
  return active_lock_;
}

void ScreenOrientationControllerImpl::DispatchEventTimerFired(TimerBase*) {
  if (!orientation_)
    return;

  ScopedAllowFullscreen allow_fullscreen(
      ScopedAllowFullscreen::kOrientationChange);
  orientation_->DispatchEvent(*Event::Create(EventTypeNames::change));
}

void ScreenOrientationControllerImpl::DidUpdateData() {
  // Do nothing.
}

void ScreenOrientationControllerImpl::RegisterWithDispatcher() {
  ScreenOrientationDispatcher::Instance().AddController(this);
}

void ScreenOrientationControllerImpl::UnregisterWithDispatcher() {
  ScreenOrientationDispatcher::Instance().RemoveController(this);
}

bool ScreenOrientationControllerImpl::HasLastData() {
  return true;
}

void ScreenOrientationControllerImpl::ContextDestroyed(ExecutionContext*) {
  StopUpdating();
  screen_orientation_service_ = nullptr;
  active_lock_ = false;
}

void ScreenOrientationControllerImpl::NotifyDispatcher() {
  if (orientation_ && GetPage()->IsPageVisible())
    StartUpdating();
  else
    StopUpdating();
}

void ScreenOrientationControllerImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(orientation_);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<LocalFrame>::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

void ScreenOrientationControllerImpl::SetScreenOrientationAssociatedPtrForTests(
    ScreenOrientationAssociatedPtr screen_orientation_associated_ptr) {
  screen_orientation_service_ = std::move(screen_orientation_associated_ptr);
}

void ScreenOrientationControllerImpl::OnLockOrientationResult(
    int request_id,
    ScreenOrientationLockResult result) {
  if (!pending_callback_ || request_id != request_id_)
    return;

  switch (result) {
    case ScreenOrientationLockResult::SCREEN_ORIENTATION_LOCK_RESULT_SUCCESS:
      pending_callback_->OnSuccess();
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_NOT_AVAILABLE:
      pending_callback_->OnError(kWebLockOrientationErrorNotAvailable);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_FULLSCREEN_REQUIRED:
      pending_callback_->OnError(kWebLockOrientationErrorFullscreenRequired);
      break;
    case ScreenOrientationLockResult::
        SCREEN_ORIENTATION_LOCK_RESULT_ERROR_CANCELED:
      pending_callback_->OnError(kWebLockOrientationErrorCanceled);
      break;
    default:
      NOTREACHED();
      break;
  }

  pending_callback_.reset();
}

void ScreenOrientationControllerImpl::CancelPendingLocks() {
  if (!pending_callback_)
    return;

  pending_callback_->OnError(kWebLockOrientationErrorCanceled);
  pending_callback_.reset();
}

int ScreenOrientationControllerImpl::GetRequestIdForTests() {
  return pending_callback_ ? request_id_ : -1;
}

}  // namespace blink
