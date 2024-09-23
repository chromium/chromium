// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_lock.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kKeyboardLockFrameDetachedErrorMsg[] =
    "Current frame is detached.";

constexpr char kKeyboardLockPromisePreemptedErrorMsg[] =
    "This request has been superseded by a subsequent lock() method call.";

constexpr char kKeyboardLockNoValidKeyCodesErrorMsg[] =
    "No valid key codes passed into lock().";

constexpr char kKeyboardLockChildFrameErrorMsg[] =
    "lock() must be called from a primary top-level browsing context.";

constexpr char kKeyboardLockRequestFailedErrorMsg[] =
    "lock() request could not be registered.";

}  // namespace

KeyboardLock::KeyboardLock(ExecutionContext* context)
    : ExecutionContextClient(context), service_(context) {}

KeyboardLock::~KeyboardLock() = default;

ScriptPromise<IDLUndefined> KeyboardLock::lock(
    ScriptState* state,
    const Vector<String>& keycodes,
    ExceptionState& exception_state) {
  DCHECK(state);

  if (!IsLocalFrameAttached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardLockFrameDetachedErrorMsg);
    return EmptyPromise();
  }

  if (!CalledFromSupportedContext(ExecutionContext::From(state))) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardLockChildFrameErrorMsg);
    return EmptyPromise();
  }

  if (!EnsureServiceConnected()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardLockRequestFailedErrorMsg);
    return EmptyPromise();
  }

  request_keylock_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(state);
  service_->RequestKeyboardLock(
      keycodes,
      WTF::BindOnce(&KeyboardLock::LockRequestFinished, WrapPersistent(this),
                    WrapPersistent(request_keylock_resolver_.Get())));
  return request_keylock_resolver_->Promise();
}

void KeyboardLock::unlock(ScriptState* state) {
  DCHECK(state);

  if (!CalledFromSupportedContext(ExecutionContext::From(state)))
    return;

  if (!EnsureServiceConnected())
    return;

  service_->CancelKeyboardLock();
}

bool KeyboardLock::IsLocalFrameAttached() {
  return DomWindow();
}

bool KeyboardLock::EnsureServiceConnected() {
  if (!service_.is_bound()) {
    if (!DomWindow())
      return false;
    // See https://bit.ly/2S0zRAS for task types.
    DomWindow()->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            DomWindow()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    DCHECK(service_.is_bound());
  }

  return true;
}

bool KeyboardLock::CalledFromSupportedContext(ExecutionContext* context) {
  DCHECK(context);
  // This API is only accessible from an outermost main frame, secure browsing
  // context.
  return DomWindow() && DomWindow()->GetFrame()->IsOutermostMainFrame() &&
         context->IsSecureContext();
}

void KeyboardLock::LockRequestFinished(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::KeyboardLockRequestResult result) {
  DCHECK(request_keylock_resolver_);

  // If |resolver| is not the current promise, then reject the promise.
  if (resolver != request_keylock_resolver_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kAbortError,
                                     kKeyboardLockPromisePreemptedErrorMsg);
    return;
  }

  switch (result) {
    case mojom::blink::KeyboardLockRequestResult::kSuccess:
      resolver->Resolve();
      break;
    case mojom::blink::KeyboardLockRequestResult::kFrameDetachedError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       kKeyboardLockFrameDetachedErrorMsg);
      break;
    case mojom::blink::KeyboardLockRequestResult::kNoValidKeyCodesError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidAccessError,
                                       kKeyboardLockNoValidKeyCodesErrorMsg);
      break;
    case mojom::blink::KeyboardLockRequestResult::kChildFrameError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       kKeyboardLockChildFrameErrorMsg);
      break;
    case mojom::blink::KeyboardLockRequestResult::kRequestFailedError:
      resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                       kKeyboardLockRequestFailedErrorMsg);
      break;
  }
  request_keylock_resolver_ = nullptr;
}

void KeyboardLock::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(request_keylock_resolver_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
