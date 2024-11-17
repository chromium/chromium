// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_orientation_lock_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/lock_orientation_callback.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

V8OrientationType::Enum ScreenOrientation::OrientationTypeToV8Enum(
    display::mojom::blink::ScreenOrientation orientation) {
  switch (orientation) {
    case display::mojom::blink::ScreenOrientation::kPortraitPrimary:
      return V8OrientationType::Enum::kPortraitPrimary;
    case display::mojom::blink::ScreenOrientation::kPortraitSecondary:
      return V8OrientationType::Enum::kPortraitSecondary;
    case display::mojom::blink::ScreenOrientation::kLandscapePrimary:
      return V8OrientationType::Enum::kLandscapePrimary;
    case display::mojom::blink::ScreenOrientation::kLandscapeSecondary:
      return V8OrientationType::Enum::kLandscapeSecondary;
    case display::mojom::blink::ScreenOrientation::kUndefined:
      break;
  }
  NOTREACHED();
}

static device::mojom::blink::ScreenOrientationLockType V8EnumToOrientationLock(
    V8OrientationLockType::Enum orientation_lock) {
  switch (orientation_lock) {
    case V8OrientationLockType::Enum::kPortraitPrimary:
      return device::mojom::blink::ScreenOrientationLockType::PORTRAIT_PRIMARY;
    case V8OrientationLockType::Enum::kPortraitSecondary:
      return device::mojom::blink::ScreenOrientationLockType::
          PORTRAIT_SECONDARY;
    case V8OrientationLockType::Enum::kLandscapePrimary:
      return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE_PRIMARY;
    case V8OrientationLockType::Enum::kLandscapeSecondary:
      return device::mojom::blink::ScreenOrientationLockType::
          LANDSCAPE_SECONDARY;
    case V8OrientationLockType::Enum::kAny:
      return device::mojom::blink::ScreenOrientationLockType::ANY;
    case V8OrientationLockType::Enum::kNatural:
      return device::mojom::blink::ScreenOrientationLockType::NATURAL;
    case V8OrientationLockType::Enum::kPortrait:
      return device::mojom::blink::ScreenOrientationLockType::PORTRAIT;
    case V8OrientationLockType::Enum::kLandscape:
      return device::mojom::blink::ScreenOrientationLockType::LANDSCAPE;
  }
  NOTREACHED();
}

// static
ScreenOrientation* ScreenOrientation::Create(LocalDOMWindow* window) {
  DCHECK(window);
  ScreenOrientation* orientation =
      MakeGarbageCollected<ScreenOrientation>(window);
  orientation->Controller()->SetOrientation(orientation);
  return orientation;
}

ScreenOrientation::ScreenOrientation(LocalDOMWindow* window)
    : ExecutionContextClient(window),
      type_(display::mojom::blink::ScreenOrientation::kUndefined),
      angle_(0) {}

ScreenOrientation::~ScreenOrientation() = default;

const WTF::AtomicString& ScreenOrientation::InterfaceName() const {
  return event_target_names::kScreenOrientation;
}

ExecutionContext* ScreenOrientation::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

V8OrientationType ScreenOrientation::type() const {
  return V8OrientationType(OrientationTypeToV8Enum(type_));
}

uint16_t ScreenOrientation::angle() const {
  return angle_;
}

void ScreenOrientation::SetType(display::mojom::blink::ScreenOrientation type) {
  type_ = type;
}

void ScreenOrientation::SetAngle(uint16_t angle) {
  angle_ = angle;
}

ScriptPromise<IDLUndefined> ScreenOrientation::lock(
    ScriptState* state,
    const V8OrientationLockType& orientation,
    ExceptionState& exception_state) {
  if (!state->ContextIsValid() || !Controller()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated to a window.");
    return EmptyPromise();
  }

  if (GetExecutionContext()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kOrientationLock)) {
    exception_state.ThrowSecurityError(
        To<LocalDOMWindow>(GetExecutionContext())
                ->GetFrame()
                ->IsInFencedFrameTree()
            ? "The window is in a fenced frame tree."
            : "The window is sandboxed and lacks the 'allow-orientation-lock' "
              "flag.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(state);
  auto promise = resolver->Promise();
  Controller()->lock(V8EnumToOrientationLock(orientation.AsEnum()),
                     std::make_unique<LockOrientationCallback>(resolver));
  return promise;
}

void ScreenOrientation::unlock() {
  if (!Controller())
    return;

  Controller()->unlock();
}

ScreenOrientationController* ScreenOrientation::Controller() {
  if (!GetExecutionContext())
    return nullptr;

  return ScreenOrientationController::From(
      *To<LocalDOMWindow>(GetExecutionContext()));
}

void ScreenOrientation::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
