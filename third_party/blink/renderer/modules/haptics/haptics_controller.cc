// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/haptics/haptics_controller.h"

#include <algorithm>

#include "base/notreached.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

namespace {

mojom::blink::HapticEffect ToMojoEffect(const V8HapticEffect& effect) {
  switch (effect.AsEnum()) {
    case V8HapticEffect::Enum::kHint:
      return mojom::blink::HapticEffect::kHint;
    case V8HapticEffect::Enum::kEdge:
      return mojom::blink::HapticEffect::kEdge;
    case V8HapticEffect::Enum::kTick:
      return mojom::blink::HapticEffect::kTick;
    case V8HapticEffect::Enum::kAlign:
      return mojom::blink::HapticEffect::kAlign;
  }
  NOTREACHED();
}

}  // namespace

// static
const char HapticsController::kSupplementName[] = "HapticsController";

// static
HapticsController& HapticsController::From(Navigator& navigator) {
  HapticsController* controller =
      Supplement<Navigator>::From<HapticsController>(navigator);
  if (!controller) {
    controller = MakeGarbageCollected<HapticsController>(navigator);
    ProvideTo(navigator, controller);
  }
  return *controller;
}

// static
void HapticsController::playHaptics(Navigator& navigator,
                                    V8HapticEffect effect,
                                    double intensity) {
  // There will be no window if it has been closed but a JavaScript reference to
  // |navigator| was retained in another window.
  if (!navigator.DomWindow()) {
    return;
  }
  From(navigator).PlayHaptics(effect, intensity);
}

HapticsController::HapticsController(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      haptics_service_(navigator.DomWindow()) {}

void HapticsController::PlayHaptics(V8HapticEffect effect, double intensity) {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window) {
    return;
  }
  LocalFrame* frame = window->GetFrame();
  if (!frame) {
    return;
  }

  if (frame->IsInFencedFrameTree()) {
    return;
  }

  // TODO(crbug.com/531787872): Gate on the "haptics" permissions policy once
  // the feature is defined.

  if (!frame->HasStickyUserActivation()) {
    return;
  }

  intensity = std::clamp(intensity, 0.0, 1.0);

  if (!haptics_service_.is_bound()) {
    window->GetBrowserInterfaceBroker().GetInterface(
        haptics_service_.BindNewPipeAndPassReceiver(
            window->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  haptics_service_->PlayHaptics(ToMojoEffect(effect), intensity);
}

void HapticsController::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
  visitor->Trace(haptics_service_);
}

}  // namespace blink
