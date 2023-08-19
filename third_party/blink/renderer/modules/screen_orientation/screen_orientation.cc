// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"

#include <memory>

#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/lock_orientation_callback.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

// This code assumes that display::mojom::blink::ScreenOrientation values are
// included in device::mojom::blink::ScreenOrientationLockType.
STATIC_ASSERT_ENUM(
    display::mojom::blink::ScreenOrientation::kPortraitPrimary,
    device::mojom::blink::ScreenOrientationLockType::PORTRAIT_PRIMARY);
STATIC_ASSERT_ENUM(
    display::mojom::blink::ScreenOrientation::kPortraitSecondary,
    device::mojom::blink::ScreenOrientationLockType::PORTRAIT_SECONDARY);
STATIC_ASSERT_ENUM(
    display::mojom::blink::ScreenOrientation::kLandscapePrimary,
    device::mojom::blink::ScreenOrientationLockType::LANDSCAPE_PRIMARY);
STATIC_ASSERT_ENUM(
    display::mojom::blink::ScreenOrientation::kLandscapeSecondary,
    device::mojom::blink::ScreenOrientationLockType::LANDSCAPE_SECONDARY);

namespace blink {

struct ScreenOrientationInfo {
  const AtomicString& name;
  device::mojom::blink::ScreenOrientationLockType orientation;
};

static ScreenOrientationInfo* OrientationsMap(unsigned& length) {
  DEFINE_STATIC_LOCAL(const AtomicString, portrait_primary,
                      ("portrait-primary"));
  DEFINE_STATIC_LOCAL(const AtomicString, portrait_secondary,
                      ("portrait-secondary"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape_primary,
                      ("landscape-primary"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape_secondary,
                      ("landscape-secondary"));
  DEFINE_STATIC_LOCAL(const AtomicString, any, ("any"));
  DEFINE_STATIC_LOCAL(const AtomicString, portrait, ("portrait"));
  DEFINE_STATIC_LOCAL(const AtomicString, landscape, ("landscape"));
  DEFINE_STATIC_LOCAL(const AtomicString, natural, ("natural"));

  static ScreenOrientationInfo orientation_map[] = {
      {portrait_primary,
       device::mojom::blink::ScreenOrientationLockType::PORTRAIT_PRIMARY},
      {portrait_secondary,
       device::mojom::blink::ScreenOrientationLockType::PORTRAIT_SECONDARY},
      {landscape_primary,
       device::mojom::blink::ScreenOrientationLockType::LANDSCAPE_PRIMARY},
      {landscape_secondary,
       device::mojom::blink::ScreenOrientationLockType::LANDSCAPE_SECONDARY},
      {any, device::mojom::blink::ScreenOrientationLockType::ANY},
      {portrait, device::mojom::blink::ScreenOrientationLockType::PORTRAIT},
      {landscape, device::mojom::blink::ScreenOrientationLockType::LANDSCAPE},
      {natural, device::mojom::blink::ScreenOrientationLockType::NATURAL}};
  length = std::size(orientation_map);

  return orientation_map;
}

const AtomicString& ScreenOrientation::OrientationTypeToString(
    display::mojom::blink::ScreenOrientation orientation) {
  unsigned length = 0;
  ScreenOrientationInfo* orientation_map = OrientationsMap(length);
  for (unsigned i = 0; i < length; ++i) {
    if (static_cast<unsigned>(orientation) ==
        static_cast<unsigned>(orientation_map[i].orientation))
      return orientation_map[i].name;
  }

  NOTREACHED();
  return g_null_atom;
}

static device::mojom::blink::ScreenOrientationLockType StringToOrientationLock(
    const AtomicString& orientation_lock_string) {
  unsigned length = 0;
  ScreenOrientationInfo* orientation_map = OrientationsMap(length);
  for (unsigned i = 0; i < length; ++i) {
    if (orientation_map[i].name == orientation_lock_string)
      return orientation_map[i].orientation;
  }

  NOTREACHED();
  return device::mojom::blink::ScreenOrientationLockType::DEFAULT;
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

String ScreenOrientation::type() const {
  return OrientationTypeToString(type_);
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

ScriptPromise ScreenOrientation::lock(ScriptState* state,
                                      const AtomicString& lock_string,
                                      ExceptionState& exception_state) {
  if (!state->ContextIsValid() || !Controller()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The object is no longer associated to a window.");
    return ScriptPromise();
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
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(state);
  ScriptPromise promise = resolver->Promise();
  Controller()->lock(StringToOrientationLock(lock_string),
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
