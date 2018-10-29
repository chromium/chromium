// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/animation_effect_or_animation_effect_sequence.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

namespace {

KeyframeEffectModelBase* CreateEffectModel() {
  StringKeyframeVector frames_mixed_properties;
  Persistent<StringKeyframe> keyframe = StringKeyframe::Create();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  keyframe = StringKeyframe::Create();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  return StringKeyframeEffectModel::Create(frames_mixed_properties);
}

KeyframeEffect* CreateKeyframeEffect(Element* element) {
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  return KeyframeEffect::Create(element, CreateEffectModel(), timing);
}

WorkletAnimation* CreateWorkletAnimation(ScriptState* script_state,
                                         Element* element) {
  AnimationEffectOrAnimationEffectSequence effects;
  AnimationEffect* effect = CreateKeyframeEffect(element);
  effects.SetAnimationEffect(effect);
  DocumentTimelineOrScrollTimeline timeline;
  scoped_refptr<SerializedScriptValue> options;

  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  return WorkletAnimation::Create(script_state, "WorkletAnimation", effects,
                                  timeline, std::move(options),
                                  exception_state);
}

}  // namespace

class WorkletAnimationTest : public RenderingTest {
 public:
  WorkletAnimationTest()
      : RenderingTest(SingleChildLocalFrameClient::Create()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    element_ = GetDocument().CreateElementForBinding("test");
    worklet_animation_ = CreateWorkletAnimation(GetScriptState(), element_);
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }

  Persistent<Element> element_;
  Persistent<WorkletAnimation> worklet_animation_;
};

TEST_F(WorkletAnimationTest, WorkletAnimationInElementAnimations) {
  DummyExceptionStateForTesting exception_state;
  worklet_animation_->play(exception_state);
  EXPECT_EQ(1u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
  worklet_animation_->cancel();
  EXPECT_EQ(0u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
}

TEST_F(WorkletAnimationTest, StyleHasCurrentAnimation) {
  scoped_refptr<ComputedStyle> style =
      GetDocument().EnsureStyleResolver().StyleForElement(element_).get();
  EXPECT_EQ(false, style->HasCurrentOpacityAnimation());
  DummyExceptionStateForTesting exception_state;
  worklet_animation_->play(exception_state);
  element_->EnsureElementAnimations().UpdateAnimationFlags(*style);
  EXPECT_EQ(true, style->HasCurrentOpacityAnimation());
}

}  //  namespace blink
