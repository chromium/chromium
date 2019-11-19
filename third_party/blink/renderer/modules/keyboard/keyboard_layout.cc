// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_layout.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kKeyboardMapFrameDetachedErrorMsg[] =
    "Current frame is detached.";

constexpr char kKeyboardMapChildFrameErrorMsg[] =
    "getLayoutMap() must be called from a top-level browsing context.";

constexpr char kKeyboardMapRequestFailedErrorMsg[] =
    "getLayoutMap() request could not be completed.";

}  // namespace

KeyboardLayout::KeyboardLayout(ExecutionContext* context)
    : ContextLifecycleObserver(context) {}

ScriptPromise KeyboardLayout::GetKeyboardLayoutMap(ScriptState* script_state) {
  DCHECK(script_state);

  if (script_promise_resolver_) {
    return script_promise_resolver_->Promise();
  }

  if (!IsLocalFrameAttached()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kKeyboardMapFrameDetachedErrorMsg));
  }

  if (!CalledFromSupportedContext(ExecutionContext::From(script_state))) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kKeyboardMapChildFrameErrorMsg));
  }

  if (!EnsureServiceConnected()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kKeyboardMapRequestFailedErrorMsg));
  }

  script_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  service_->GetKeyboardLayoutMap(
      WTF::Bind(&KeyboardLayout::GotKeyboardLayoutMap, WrapPersistent(this),
                WrapPersistent(script_promise_resolver_.Get())));
  return script_promise_resolver_->Promise();
}

bool KeyboardLayout::IsLocalFrameAttached() {
  if (GetFrame())
    return true;
  return false;
}

bool KeyboardLayout::EnsureServiceConnected() {
  if (!service_) {
    LocalFrame* frame = GetFrame();
    if (!frame) {
      return false;
    }
    frame->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver());
    DCHECK(service_);
  }
  return true;
}

bool KeyboardLayout::CalledFromSupportedContext(ExecutionContext* context) {
  DCHECK(context);
  // This API is only accessible from a top level, secure browsing context.
  LocalFrame* frame = GetFrame();
  return frame && frame->IsMainFrame() && context->IsSecureContext();
}

void KeyboardLayout::GotKeyboardLayoutMap(
    ScriptPromiseResolver* resolver,
    mojom::blink::GetKeyboardLayoutMapResultPtr result) {
  DCHECK(script_promise_resolver_);

  switch (result->status) {
    case mojom::blink::GetKeyboardLayoutMapStatus::kSuccess:
      script_promise_resolver_->Resolve(
          MakeGarbageCollected<KeyboardLayoutMap>(result->layout_map));
      break;
    case mojom::blink::GetKeyboardLayoutMapStatus::kFail:
      script_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          kKeyboardMapRequestFailedErrorMsg));
      break;
  }

  script_promise_resolver_ = nullptr;
}

void KeyboardLayout::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_promise_resolver_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
