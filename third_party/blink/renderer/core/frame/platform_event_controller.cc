// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/platform_event_controller.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PlatformEventController::PlatformEventController(LocalDOMWindow& window)
    : PageVisibilityObserver(window.GetFrame()->GetPage()),
      has_event_listener_(false),
      is_active_(false),
      window_(window) {}

PlatformEventController::~PlatformEventController() = default;

void PlatformEventController::UpdateCallback() {
  DCHECK(HasLastData());
  DidUpdateData();
}

void PlatformEventController::StartUpdating() {
  if (is_active_ || !window_)
    return;

  if (HasLastData() && !update_callback_handle_.IsActive()) {
    update_callback_handle_ = PostCancellableTask(
        *window_->GetTaskRunner(TaskType::kInternalDefault), FROM_HERE,
        WTF::BindOnce(&PlatformEventController::UpdateCallback,
                      WrapWeakPersistent(this)));
  }

  RegisterWithDispatcher();
  is_active_ = true;
}

void PlatformEventController::StopUpdating() {
  if (!is_active_)
    return;

  update_callback_handle_.Cancel();
  UnregisterWithDispatcher();
  is_active_ = false;
}

void PlatformEventController::PageVisibilityChanged() {
  if (!has_event_listener_)
    return;

  if (GetPage()->IsPageVisible())
    StartUpdating();
  else
    StopUpdating();
}

void PlatformEventController::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  PageVisibilityObserver::Trace(visitor);
}

}  // namespace blink
