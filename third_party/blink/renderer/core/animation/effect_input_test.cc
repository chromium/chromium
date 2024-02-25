// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/effect_input.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

Element* AppendElement(Document& document) {
  Element* element = document.CreateElementForBinding(AtomicString("foo"));
  document.documentElement()->AppendChild(element);
  return element;
}

TEST(AnimationEffectInputTest, SortedOffsets) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                                 .AddString("width", "100px")
                                                 .AddString("offset", "0")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "0px")
                                                 .AddString("offset", "1")
                                                 .GetScriptValue()};

  ScriptValue js_keyframes(
      scope.GetIsolate(),
      ToV8Traits<IDLSequence<IDLObject>>::ToV8(script_state, blink_keyframes));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element, js_keyframes, EffectModel::kCompositeReplace, script_state,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1.0, effect->GetFrames()[1]->CheckedOffset());
}

TEST(AnimationEffectInputTest, UnsortedOffsets) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                                 .AddString("width", "0px")
                                                 .AddString("offset", "1")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "100px")
                                                 .AddString("offset", "0")
                                                 .GetScriptValue()};

  ScriptValue js_keyframes(
      scope.GetIsolate(),
      ToV8Traits<IDLSequence<IDLObject>>::ToV8(script_state, blink_keyframes));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       script_state, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

TEST(AnimationEffectInputTest, LooslySorted) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                                 .AddString("width", "100px")
                                                 .AddString("offset", "0")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "200px")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "0px")
                                                 .AddString("offset", "1")
                                                 .GetScriptValue()};

  ScriptValue js_keyframes(
      scope.GetIsolate(),
      ToV8Traits<IDLSequence<IDLObject>>::ToV8(script_state, blink_keyframes));

  Element* element = AppendElement(scope.GetDocument());
  KeyframeEffectModelBase* effect = EffectInput::Convert(
      element, js_keyframes, EffectModel::kCompositeReplace, script_state,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(1, effect->GetFrames()[2]->CheckedOffset());
}

TEST(AnimationEffectInputTest, OutOfOrderWithNullOffsets) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  HeapVector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                                 .AddString("height", "100px")
                                                 .AddString("offset", "0.5")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("height", "150px")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("height", "200px")
                                                 .AddString("offset", "0")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("height", "300px")
                                                 .AddString("offset", "1")
                                                 .GetScriptValue()};

  ScriptValue js_keyframes(
      scope.GetIsolate(),
      ToV8Traits<IDLSequence<IDLObject>>::ToV8(script_state, blink_keyframes));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       script_state, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(AnimationEffectInputTest, Invalid) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Not loosely sorted by offset, and there exists a keyframe with null offset.
  HeapVector<ScriptValue> blink_keyframes = {V8ObjectBuilder(script_state)
                                                 .AddString("width", "0px")
                                                 .AddString("offset", "1")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "200px")
                                                 .GetScriptValue(),
                                             V8ObjectBuilder(script_state)
                                                 .AddString("width", "200px")
                                                 .AddString("offset", "0")
                                                 .GetScriptValue()};

  ScriptValue js_keyframes(
      scope.GetIsolate(),
      ToV8Traits<IDLSequence<IDLObject>>::ToV8(script_state, blink_keyframes));

  Element* element = AppendElement(scope.GetDocument());
  EffectInput::Convert(element, js_keyframes, EffectModel::kCompositeReplace,
                       script_state, scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

}  // namespace blink
