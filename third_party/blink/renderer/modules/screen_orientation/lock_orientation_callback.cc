// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/lock_orientation_callback.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

LockOrientationCallback::LockOrientationCallback(
    ScriptPromiseResolver* resolver)
    : resolver_(resolver) {}

LockOrientationCallback::~LockOrientationCallback() = default;

void LockOrientationCallback::OnSuccess() {
  // Resolving the promise should be done after the event is fired which is
  // delayed to avoid running script on the stack. We then have to delay
  // resolving the promise.
  resolver_->GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::Bind(
                                [](ScriptPromiseResolver* resolver) {
                                  resolver->Resolve();
                                },
                                std::move(resolver_)));
}

void LockOrientationCallback::OnError(WebLockOrientationError error) {
  DOMExceptionCode code = DOMExceptionCode::kUnknownError;
  String message = "";

  switch (error) {
    case kWebLockOrientationErrorNotAvailable:
      code = DOMExceptionCode::kNotSupportedError;
      message = "screen.orientation.lock() is not available on this device.";
      break;
    case kWebLockOrientationErrorFullscreenRequired:
      code = DOMExceptionCode::kSecurityError;
      message =
          "The page needs to be fullscreen in order to call "
          "screen.orientation.lock().";
      break;
    case kWebLockOrientationErrorCanceled:
      code = DOMExceptionCode::kAbortError;
      message =
          "A call to screen.orientation.lock() or screen.orientation.unlock() "
          "canceled this call.";
      break;
  }

  resolver_->Reject(MakeGarbageCollected<DOMException>(code, message));
}

}  // namespace blink
