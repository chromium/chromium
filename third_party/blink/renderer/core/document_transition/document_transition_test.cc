// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_init.h"
#include "third_party/blink/renderer/core/document_transition/document_create_transition.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class DocumentTransitionTest : public RenderingTest,
                               private ScopedDocumentTransitionForTest {
 public:
  DocumentTransitionTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()),
        ScopedDocumentTransitionForTest(true) {}
};

TEST_F(DocumentTransitionTest, NewTransitionCreated) {
  DocumentTransitionInit init;
  auto* first_transition =
      DocumentCreateTransition::createTransition(GetDocument(), &init);
  auto* second_transition =
      DocumentCreateTransition::createTransition(GetDocument(), &init);

  EXPECT_TRUE(first_transition);
  EXPECT_TRUE(second_transition);
  EXPECT_NE(first_transition, second_transition);
}

TEST_F(DocumentTransitionTest, TransitionPreparePromiseResolves) {
  DocumentTransitionInit init;
  auto* transition =
      DocumentCreateTransition::createTransition(GetDocument(), &init);
  ASSERT_TRUE(transition);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();

  ScriptPromiseTester promise_tester(script_state,
                                     transition->prepare(script_state));

  UpdateAllLifecyclePhasesForTest();
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsFulfilled());
}

}  // namespace blink
