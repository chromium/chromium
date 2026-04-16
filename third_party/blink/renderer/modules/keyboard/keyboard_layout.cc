// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_layout.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/keyboard/keyboard_layout_map.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

constexpr char kKeyboardMapFrameDetachedErrorMsg[] =
    "Current frame is detached.";

constexpr char kFeaturePolicyBlocked[] =
    "getLayoutMap() must be called from a top-level browsing context or "
    "allowed by the permission policy.";

constexpr char kKeyboardMapRequestFailedErrorMsg[] =
    "getLayoutMap() request could not be completed.";

}

KeyboardLayout::KeyboardLayout(ExecutionContext* context)
    : ExecutionContextClient(context), service_(context) {}

ScriptPromise<KeyboardLayoutMap> KeyboardLayout::GetKeyboardLayoutMap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(script_state);

  if (!IsLocalFrameAttached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardMapFrameDetachedErrorMsg);
    return EmptyPromise();
  }

  if (!EnsureServiceConnected()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardMapRequestFailedErrorMsg);
    return EmptyPromise();
  }

  if (!layout_map_property_) {
    layout_map_property_ = MakeGarbageCollected<LayoutMapProperty>(
        ExecutionContext::From(script_state));
  }

  auto promise = layout_map_property_->Promise(script_state->World());

  if (!is_request_pending_) {
    is_request_pending_ = true;
    service_->GetKeyboardLayoutMap(
        BindOnce(&KeyboardLayout::GotKeyboardLayoutMap, WrapPersistent(this)));
  }
  return promise;
}

bool KeyboardLayout::IsLocalFrameAttached() {
  return DomWindow();
}

bool KeyboardLayout::EnsureServiceConnected() {
  if (!service_.is_bound()) {
    if (!DomWindow()) {
      return false;
    }
    DomWindow()->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            DomWindow()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    DCHECK(service_.is_bound());
  }
  return true;
}

void KeyboardLayout::GotKeyboardLayoutMap(
    mojom::blink::GetKeyboardLayoutMapResultPtr result) {
  DCHECK(layout_map_property_);
  DCHECK(is_request_pending_);
  is_request_pending_ = false;

  switch (result->status) {
    case mojom::blink::GetKeyboardLayoutMapStatus::kSuccess:
      layout_map_property_->Resolve(
          MakeGarbageCollected<KeyboardLayoutMap>(result->layout_map));
      break;
    case mojom::blink::GetKeyboardLayoutMapStatus::kFail:
      layout_map_property_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError,
          kKeyboardMapRequestFailedErrorMsg));
      break;
    case mojom::blink::GetKeyboardLayoutMapStatus::kDenied:
      layout_map_property_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, kFeaturePolicyBlocked));
      break;
  }
  layout_map_property_ = nullptr;
}

void KeyboardLayout::Trace(Visitor* visitor) const {
  visitor->Trace(layout_map_property_);
  visitor->Trace(service_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
