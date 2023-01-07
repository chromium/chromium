// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/css_animation_worklet.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
AnimationWorklet* CSSAnimationWorklet::animationWorklet(
    ScriptState* script_state) {
  LocalDOMWindow* window = ToLocalDOMWindow(script_state->GetContext());

  if (!window->GetFrame())
    return nullptr;
  return From(*window).animation_worklet_.Get();
}

// Break the following cycle when the context gets detached.
// Otherwise, the worklet object will leak.
//
// window => CSS.animationWorklet
// => CSSAnimationWorklet
// => AnimationWorklet  <--- break this reference
// => ThreadedWorkletMessagingProxy
// => Document
// => ... => window
void CSSAnimationWorklet::ContextDestroyed() {
  animation_worklet_ = nullptr;
}

void CSSAnimationWorklet::Trace(Visitor* visitor) const {
  visitor->Trace(animation_worklet_);
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

// static
CSSAnimationWorklet& CSSAnimationWorklet::From(LocalDOMWindow& window) {
  CSSAnimationWorklet* supplement =
      Supplement<LocalDOMWindow>::From<CSSAnimationWorklet>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<CSSAnimationWorklet>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

CSSAnimationWorklet::CSSAnimationWorklet(LocalDOMWindow& window)
    : Supplement(window),
      ExecutionContextLifecycleObserver(&window),
      animation_worklet_(MakeGarbageCollected<AnimationWorklet>(window)) {
  DCHECK(GetExecutionContext());
}

const char CSSAnimationWorklet::kSupplementName[] = "CSSAnimationWorklet";

}  // namespace blink
