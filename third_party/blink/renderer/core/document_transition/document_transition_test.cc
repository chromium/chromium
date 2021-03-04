// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_root_transition_type.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class DocumentTransitionTest : public RenderingTest,
                               private ScopedDocumentTransitionForTest {
 public:
  DocumentTransitionTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedDocumentTransitionForTest(true) {}

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  // Testing the compositor interaction is not in scope for these unittests. So,
  // instead of setting up a full commit flow, simulate it by calling the commit
  // callback directly.
  void UpdateAllLifecyclePhasesAndSimulateCommit() {
    UpdateAllLifecyclePhasesForTest();
    for (auto& request : GetChromeClient()
                             .layer_tree_host()
                             ->TakeDocumentTransitionRequestsForTesting()) {
      request->TakeFinishedCallback().Run();
    }
  }

  using State = DocumentTransition::State;

  State GetState(DocumentTransition* transition) const {
    return transition->state_;
  }
};

TEST_F(DocumentTransitionTest, TransitionObjectPersists) {
  auto* first_transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  auto* second_transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  EXPECT_TRUE(first_transition);
  EXPECT_EQ(GetState(first_transition), State::kIdle);
  EXPECT_TRUE(second_transition);
  EXPECT_EQ(first_transition, second_transition);
}

TEST_F(DocumentTransitionTest, TransitionPreparePromiseResolves) {
  DocumentTransitionInit init;
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  ASSERT_TRUE(transition);
  EXPECT_EQ(GetState(transition), State::kIdle);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester promise_tester(
      script_state, transition->prepare(script_state, &init, exception_state));

  EXPECT_EQ(GetState(transition), State::kPreparing);
  UpdateAllLifecyclePhasesAndSimulateCommit();
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_F(DocumentTransitionTest, AdditionalPrepareRejectsPreviousPromise) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionInit init;
  ScriptPromiseTester first_promise_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  ScriptPromiseTester second_promise_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndSimulateCommit();
  first_promise_tester.WaitUntilSettled();
  second_promise_tester.WaitUntilSettled();

  EXPECT_TRUE(first_promise_tester.IsRejected());
  EXPECT_TRUE(second_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_F(DocumentTransitionTest, EffectParsing) {
  // Test default init.
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  DocumentTransitionInit default_init;
  transition->prepare(script_state, &default_init, exception_state);

  auto request = transition->TakePendingRequest();
  ASSERT_TRUE(request);

  auto directive = request->ConstructDirective();
  EXPECT_EQ(directive.effect(), DocumentTransition::Request::Effect::kNone);

  // Test "explode" effect parsing.
  DocumentTransitionInit explode_init;
  explode_init.setRootTransition(
      V8RootTransitionType(V8RootTransitionType::Enum::kExplode));
  transition->prepare(script_state, &explode_init, exception_state);

  request = transition->TakePendingRequest();
  ASSERT_TRUE(request);

  directive = request->ConstructDirective();
  EXPECT_EQ(directive.effect(), DocumentTransition::Request::Effect::kExplode);

  // Test invalid effect parsing.
  DocumentTransitionInit invalid_init;
  invalid_init.setRootTransition("invalid effect");
  transition->prepare(script_state, &invalid_init, exception_state);

  request = transition->TakePendingRequest();
  ASSERT_TRUE(request);

  directive = request->ConstructDirective();
  EXPECT_EQ(directive.effect(), DocumentTransition::Request::Effect::kNone);
}

TEST_F(DocumentTransitionTest, AdditionalPrepareAfterPreparedSucceeds) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionInit init;
  ScriptPromiseTester first_promise_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndSimulateCommit();
  first_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(first_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  ScriptPromiseTester second_promise_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndSimulateCommit();
  second_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(second_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_F(DocumentTransitionTest, TransitionCleanedUpBeforePromiseResolution) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionInit init;
  ScriptPromiseTester tester(
      script_state,
      DocumentTransitionSupplement::documentTransition(GetDocument())
          ->prepare(script_state, &init, exception_state));

  // ActiveScriptWrappable should keep the transition alive.
  ThreadState::Current()->CollectAllGarbageForTesting();

  UpdateAllLifecyclePhasesAndSimulateCommit();
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(DocumentTransitionTest, StartHasNoEffectUnlessPrepared) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  EXPECT_EQ(GetState(transition), State::kIdle);
  EXPECT_FALSE(transition->TakePendingRequest());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  transition->start(script_state, exception_state);
  EXPECT_EQ(GetState(transition), State::kIdle);
  EXPECT_FALSE(transition->TakePendingRequest());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(DocumentTransitionTest, StartAfterPrepare) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionInit init;
  ScriptPromiseTester prepare_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndSimulateCommit();
  prepare_tester.WaitUntilSettled();
  EXPECT_TRUE(prepare_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  ScriptPromiseTester start_tester(
      script_state, transition->start(script_state, exception_state));
  // Take the request.
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  // Subsequent starts should get an exception.
  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(transition->TakePendingRequest());

  start_request->TakeFinishedCallback().Run();
  EXPECT_EQ(GetState(transition), State::kIdle);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
}

TEST_F(DocumentTransitionTest, StartPromiseIsResolved) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionInit init;
  ScriptPromiseTester prepare_tester(
      script_state, transition->prepare(script_state, &init, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndSimulateCommit();
  prepare_tester.WaitUntilSettled();
  EXPECT_TRUE(prepare_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  ScriptPromiseTester start_tester(
      script_state, transition->start(script_state, exception_state));

  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndSimulateCommit();

  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

}  // namespace blink
