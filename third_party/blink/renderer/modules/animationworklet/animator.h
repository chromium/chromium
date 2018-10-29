// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_H_

#include "third_party/blink/renderer/modules/animationworklet/worklet_group_effect_proxy.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutators_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "v8/include/v8.h"

namespace blink {

class AnimatorDefinition;
class ScriptState;

// Represents an animator instance. It owns the underlying |v8::Object| for the
// instance and knows how to invoke the |animate| function on it.
// See also |AnimationWorkletGlobalScope::CreateInstance|.
class Animator final : public GarbageCollectedFinalized<Animator>,
                       public NameClient {
 public:
  Animator(v8::Isolate*,
           AnimatorDefinition*,
           v8::Local<v8::Value> instance,
           int num_effects);
  ~Animator();
  void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override { return "Animator"; }

  // Returns true if it successfully invoked animate callback in JS. It receives
  // latest state coming from |AnimationHost| as input and fills
  // the output state with new updates.
  bool Animate(ScriptState*,
               double current_time,
               AnimationWorkletDispatcherOutput::AnimationState*);
  std::vector<base::Optional<TimeDelta>> GetLocalTimes() const;

 private:
  // This object keeps the definition object, and animator instance alive.
  // It participates in wrapper tracing as it holds onto V8 wrappers.
  TraceWrapperMember<AnimatorDefinition> definition_;
  TraceWrapperV8Reference<v8::Value> instance_;

  Member<WorkletGroupEffectProxy> group_effect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATOR_H_
