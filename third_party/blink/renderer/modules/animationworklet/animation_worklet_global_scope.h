// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_GLOBAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_GLOBAL_SCOPE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/modules/animationworklet/animator.h"
#include "third_party/blink/renderer/modules/animationworklet/animator_definition.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutators_state.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ExceptionState;
class WorkletAnimationOptions;

// Represents the animation worklet global scope and implements all methods that
// the global scope exposes to user script (See
// |AnimationWorkletGlobalScope.idl|). The instances of this class live on the
// worklet thread but have a corresponding proxy on the main thread which is
// accessed by the animation worklet instance. User scripts can register
// animator definitions with the global scope (via |registerAnimator| method).
// The scope keeps a map of these animator definitions and can look them up
// based on their name. The scope also owns a list of active animators that it
// animates.
class MODULES_EXPORT AnimationWorkletGlobalScope : public WorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationWorkletGlobalScope* Create(
      std::unique_ptr<GlobalScopeCreationParams>,
      WorkerThread*);
  ~AnimationWorkletGlobalScope() override;
  void Trace(blink::Visitor*) override;
  void Dispose() override;
  bool IsAnimationWorkletGlobalScope() const final { return true; }

  // Invokes the |animate| function of all of its active animators.
  std::unique_ptr<AnimationWorkletOutput> Mutate(const AnimationWorkletInput&);

  // Registers a animator definition with the given name and constructor.
  void registerAnimator(const String& name,
                        const ScriptValue& constructor_value,
                        ExceptionState&);

  AnimatorDefinition* FindDefinitionForTest(const String& name);
  unsigned GetAnimatorsSizeForTest() { return animators_.size(); }

 private:
  AnimationWorkletGlobalScope(std::unique_ptr<GlobalScopeCreationParams>,
                              WorkerThread*);

  void RegisterWithProxyClientIfNeeded();
  Animator* CreateInstance(const String& name,
                           WorkletAnimationOptions* options,
                           int num_effects);
  Animator* CreateAnimatorFor(int animation_id,
                              const String& name,
                              WorkletAnimationOptions* options,
                              int num_effects);
  typedef HeapHashMap<String, TraceWrapperMember<AnimatorDefinition>>
      DefinitionMap;
  DefinitionMap animator_definitions_;

  typedef HeapHashMap<int, TraceWrapperMember<Animator>> AnimatorMap;
  AnimatorMap animators_;

  bool registered_ = false;
};

template <>
struct DowncastTraits<AnimationWorkletGlobalScope> {
  static bool AllowFrom(const ExecutionContext& context) {
    return context.IsAnimationWorkletGlobalScope();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_ANIMATION_WORKLET_GLOBAL_SCOPE_H_
