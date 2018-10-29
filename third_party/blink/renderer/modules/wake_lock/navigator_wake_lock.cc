// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/navigator_wake_lock.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock.h"

namespace blink {

// static
const char NavigatorWakeLock::kSupplementName[] = "NavigatorWakeLock";

NavigatorWakeLock::NavigatorWakeLock(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
ScriptPromise NavigatorWakeLock::getWakeLock(ScriptState* script_state,
                                             Navigator& navigator,
                                             String lock_type) {
  return NavigatorWakeLock::From(navigator).getWakeLock(script_state,
                                                        lock_type);
}

ScriptPromise NavigatorWakeLock::getWakeLock(ScriptState* script_state,
                                             String lock_type) {
  if (lock_type == "screen") {
    if (!wake_lock_screen_)
      wake_lock_screen_ = WakeLock::CreateScreenWakeLock(script_state);
    return wake_lock_screen_->GetPromise(script_state);
  } else if (lock_type == "system") {
    if (!wake_lock_system_)
      wake_lock_system_ = WakeLock::CreateSystemWakeLock(script_state);
    return wake_lock_system_->GetPromise(script_state);
  }

  return ScriptPromise::RejectWithDOMException(
      script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                         "WakeLockType Not Supported"));
}

NavigatorWakeLock& NavigatorWakeLock::From(Navigator& navigator) {
  NavigatorWakeLock* supplement =
      Supplement<Navigator>::From<NavigatorWakeLock>(navigator);
  if (!supplement) {
    supplement = new NavigatorWakeLock(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

void NavigatorWakeLock::Trace(blink::Visitor* visitor) {
  visitor->Trace(wake_lock_screen_);
  visitor->Trace(wake_lock_system_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
