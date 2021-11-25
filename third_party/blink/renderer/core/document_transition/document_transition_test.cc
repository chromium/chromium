// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "base/test/scoped_feature_list.h"
#include "cc/document_transition/document_transition_request.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_prepare_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_start_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_root_transition_type.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_state.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class DocumentTransitionTest : public testing::Test,
                               public PaintTestConfigurations,
                               private ScopedDocumentTransitionForTest {
 public:
  DocumentTransitionTest() : ScopedDocumentTransitionForTest(true) {}

  static void ConfigureCompositingWebView(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize(nullptr, nullptr,
                                 &ConfigureCompositingWebView);
    web_view_helper_->Resize(gfx::Size(200, 200));
  }

  void TearDown() override { web_view_helper_.reset(); }

  Document& GetDocument() {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->GetDocument();
  }

  bool ElementIsComposited(const char* id) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return !CcLayersByDOMElementId(RootCcLayer(), id).IsEmpty();

    auto* element = GetDocument().getElementById(id);
    if (!element)
      return false;

    auto* box = element->GetLayoutBox();
    return box && box->HasSelfPaintingLayer() &&
           box->Layer()->GetCompositingState() == kPaintsIntoOwnBacking;
  }

  // Testing the compositor interaction is not in scope for these unittests. So,
  // instead of setting up a full commit flow, simulate it by calling the commit
  // callback directly.
  void UpdateAllLifecyclePhasesAndFinishDirectives() {
    UpdateAllLifecyclePhasesForTest();
    for (auto& callback :
         LayerTreeHost()->TakeDocumentTransitionCallbacksForTesting()) {
      std::move(callback).Run();
    }
  }

  cc::LayerTreeHost* LayerTreeHost() {
    return web_view_helper_->LocalMainFrame()
        ->FrameWidgetImpl()
        ->LayerTreeHostForTesting();
  }

  const cc::Layer* RootCcLayer() {
    return paint_artifact_compositor()->RootLayer();
  }

  LocalFrameView* GetLocalFrameView() {
    return web_view_helper_->LocalMainFrame()->GetFrameView();
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    return GetLocalFrameView()->GetPaintArtifactCompositor();
  }

  void SetHtmlInnerHTML(const String& content) {
    GetDocument().body()->setInnerHTML(content);
    UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesForTest() {
    web_view_helper_->GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  using State = DocumentTransition::State;

  State GetState(DocumentTransition* transition) const {
    return transition->state_;
  }

 private:
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(DocumentTransitionTest);

TEST_P(DocumentTransitionTest, TransitionObjectPersists) {
  auto* first_transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  auto* second_transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  EXPECT_TRUE(first_transition);
  EXPECT_EQ(GetState(first_transition), State::kIdle);
  EXPECT_TRUE(second_transition);
  EXPECT_EQ(first_transition, second_transition);
}

TEST_P(DocumentTransitionTest, TransitionPreparePromiseResolves) {
  DocumentTransitionPrepareOptions options;
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  ASSERT_TRUE(transition);
  EXPECT_EQ(GetState(transition), State::kIdle);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester promise_tester(
      script_state,
      transition->prepare(script_state, &options, exception_state));

  EXPECT_EQ(GetState(transition), State::kPreparing);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_P(DocumentTransitionTest, AdditionalPrepareRejectsPreviousPromise) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions options;
  ScriptPromiseTester first_promise_tester(
      script_state,
      transition->prepare(script_state, &options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  ScriptPromiseTester second_promise_tester(
      script_state,
      transition->prepare(script_state, &options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  first_promise_tester.WaitUntilSettled();
  second_promise_tester.WaitUntilSettled();

  EXPECT_TRUE(first_promise_tester.IsRejected());
  EXPECT_TRUE(second_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_P(DocumentTransitionTest, EffectParsing) {
  // Test default init.
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  DocumentTransitionPrepareOptions default_options;
  transition->prepare(script_state, &default_options, exception_state);

  auto request = transition->TakePendingRequest();
  ASSERT_TRUE(request);

  auto directive = request->ConstructDirective({});
  EXPECT_EQ(directive.effect(), DocumentTransition::Request::Effect::kNone);

  // Test "explode" effect parsing.
  DocumentTransitionPrepareOptions explode_options;
  explode_options.setRootTransition(
      V8RootTransitionType(V8RootTransitionType::Enum::kExplode));
  transition->prepare(script_state, &explode_options, exception_state);

  request = transition->TakePendingRequest();
  ASSERT_TRUE(request);

  directive = request->ConstructDirective({});
  EXPECT_EQ(directive.effect(), DocumentTransition::Request::Effect::kExplode);
}

TEST_P(DocumentTransitionTest, PrepareSharedElementsWantToBeComposited) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div { width: 100px; height: 100px; contain: paint }
    </style>

    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById("e1");
  auto* e2 = GetDocument().getElementById("e2");
  auto* e3 = GetDocument().getElementById("e3");

  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  EXPECT_FALSE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());

  DocumentTransitionPrepareOptions options;
  // Set two of the elements to be shared.
  options.setSharedElements({e1, e3});
  transition->prepare(script_state, &options, exception_state);

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_TRUE(e3->ShouldCompositeForDocumentTransition());

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_TRUE(ElementIsComposited("e3"));

  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());

  // We need to actually run the lifecycle in order to see the full effect of
  // finishing directives.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_FALSE(ElementIsComposited("e3"));
}

TEST_P(DocumentTransitionTest, UncontainedElementsAreCleared) {
  SetHtmlInnerHTML(R"HTML(
    <style>#e1 { width: 100px; height: 100px; contain: paint }</style>
    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById("e1");
  auto* e2 = GetDocument().getElementById("e2");
  auto* e3 = GetDocument().getElementById("e3");

  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  EXPECT_FALSE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());

  DocumentTransitionPrepareOptions options;
  options.setSharedElements({e1, e2, e3});
  transition->prepare(script_state, &options, exception_state);

  EXPECT_TRUE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_TRUE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_TRUE(e3->ShouldCompositeForDocumentTransition());

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  // Since only the first element is contained, the rest should be cleared.
  EXPECT_TRUE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_FALSE(ElementIsComposited("e3"));
}

TEST_P(DocumentTransitionTest, StartSharedElementCountMismatch) {
  SetHtmlInnerHTML(R"HTML(
    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById("e1");
  auto* e2 = GetDocument().getElementById("e2");
  auto* e3 = GetDocument().getElementById("e3");

  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions prepare_options;
  // Set two of the elements to be shared.
  prepare_options.setSharedElements({e1, e3});
  transition->prepare(script_state, &prepare_options, exception_state);

  UpdateAllLifecyclePhasesAndFinishDirectives();

  DocumentTransitionStartOptions start_options;
  // Set all of the elements as shared. This should cause an exception.
  start_options.setSharedElements({e1, e2, e3});

  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, &start_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  EXPECT_FALSE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());
}

TEST_P(DocumentTransitionTest, StartSharedElementsWantToBeComposited) {
  SetHtmlInnerHTML(R"HTML(
    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById("e1");
  auto* e2 = GetDocument().getElementById("e2");
  auto* e3 = GetDocument().getElementById("e3");

  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions prepare_options;
  // Set two of the elements to be shared.
  prepare_options.setSharedElements({e1, e3});
  transition->prepare(script_state, &prepare_options, exception_state);

  EXPECT_TRUE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_TRUE(e3->ShouldCompositeForDocumentTransition());

  UpdateAllLifecyclePhasesAndFinishDirectives();

  DocumentTransitionStartOptions start_options;
  // Set two different elements as shared.
  start_options.setSharedElements({e1, e2});
  transition->start(script_state, &start_options, exception_state);

  EXPECT_TRUE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_TRUE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());

  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(e1->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e2->ShouldCompositeForDocumentTransition());
  EXPECT_FALSE(e3->ShouldCompositeForDocumentTransition());
}

TEST_P(DocumentTransitionTest, AdditionalPrepareAfterPreparedSucceeds) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions options;
  ScriptPromiseTester first_promise_tester(
      script_state,
      transition->prepare(script_state, &options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  first_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(first_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  ScriptPromiseTester second_promise_tester(
      script_state,
      transition->prepare(script_state, &options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  second_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(second_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);
}

TEST_P(DocumentTransitionTest, TransitionCleanedUpBeforePromiseResolution) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions options;
  ScriptPromiseTester tester(
      script_state,
      DocumentTransitionSupplement::documentTransition(GetDocument())
          ->prepare(script_state, &options, exception_state));

  // ActiveScriptWrappable should keep the transition alive.
  ThreadState::Current()->CollectAllGarbageForTesting();

  UpdateAllLifecyclePhasesAndFinishDirectives();
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_P(DocumentTransitionTest, StartHasNoEffectUnlessPrepared) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());
  EXPECT_EQ(GetState(transition), State::kIdle);
  EXPECT_FALSE(transition->TakePendingRequest());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionStartOptions options;
  transition->start(script_state, &options, exception_state);
  EXPECT_EQ(GetState(transition), State::kIdle);
  EXPECT_FALSE(transition->TakePendingRequest());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_P(DocumentTransitionTest, StartAfterPrepare) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions prepare_options;
  ScriptPromiseTester prepare_tester(
      script_state,
      transition->prepare(script_state, &prepare_options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  prepare_tester.WaitUntilSettled();
  EXPECT_TRUE(prepare_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  DocumentTransitionStartOptions start_options;
  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, &start_options, exception_state));
  // Take the request.
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  // Subsequent starts should get an exception.
  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, &start_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(transition->TakePendingRequest());

  start_request->TakeFinishedCallback().Run();
  EXPECT_EQ(GetState(transition), State::kIdle);
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
}

TEST_P(DocumentTransitionTest, StartPromiseIsResolved) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  DocumentTransitionPrepareOptions prepare_options;
  ScriptPromiseTester prepare_tester(
      script_state,
      transition->prepare(script_state, &prepare_options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  // Visual updates are allows during prepare phase.
  EXPECT_FALSE(LayerTreeHost()->IsDeferringCommits());

  UpdateAllLifecyclePhasesAndFinishDirectives();
  prepare_tester.WaitUntilSettled();
  EXPECT_TRUE(prepare_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kPrepared);

  // Visual updates are stalled between prepared and start.
  EXPECT_TRUE(LayerTreeHost()->IsDeferringCommits());

  DocumentTransitionStartOptions start_options;
  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, &start_options, exception_state));

  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // Visual updates are restored on start.
  EXPECT_FALSE(LayerTreeHost()->IsDeferringCommits());

  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

TEST_P(DocumentTransitionTest, AbortSignal) {
  auto* transition =
      DocumentTransitionSupplement::documentTransition(GetDocument());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  auto* abort_signal =
      MakeGarbageCollected<AbortSignal>(v8_scope.GetExecutionContext());
  DocumentTransitionPrepareOptions prepare_options;
  prepare_options.setAbortSignal(abort_signal);
  ScriptPromiseTester prepare_tester(
      script_state,
      transition->prepare(script_state, &prepare_options, exception_state));
  EXPECT_EQ(GetState(transition), State::kPreparing);

  abort_signal->SignalAbort(script_state);
  prepare_tester.WaitUntilSettled();
  EXPECT_TRUE(prepare_tester.IsRejected());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

}  // namespace blink
