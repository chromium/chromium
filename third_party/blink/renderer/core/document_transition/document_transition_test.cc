// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "cc/document_transition/document_transition_request.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_set_element_options.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_utils.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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
    return !CcLayersByDOMElementId(RootCcLayer(), id).IsEmpty();
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

  void FinishTransition() {
    auto* transition =
        DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
    transition->NotifyStartFinished(transition->last_start_sequence_id_);
  }

  bool ShouldCompositeForDocumentTransition(Element* e) {
    auto* layout_object = e->GetLayoutObject();
    auto* transition =
        DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
    return layout_object && transition &&
           transition->IsTransitionParticipant(*layout_object);
  }

  void ValidatePseudoElementTree(
      const Vector<WTF::AtomicString>& document_transition_tags,
      bool has_incoming_image) {
    auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
        kPseudoIdPageTransition);
    ASSERT_TRUE(transition_pseudo);
    EXPECT_TRUE(transition_pseudo->GetComputedStyle());
    EXPECT_TRUE(transition_pseudo->GetLayoutObject());

    PseudoElement* previous_container = nullptr;
    for (const auto& document_transition_tag : document_transition_tags) {
      SCOPED_TRACE(document_transition_tag);
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionContainer, document_transition_tag);
      ASSERT_TRUE(container_pseudo);
      EXPECT_TRUE(container_pseudo->GetComputedStyle());
      EXPECT_TRUE(container_pseudo->GetLayoutObject());

      if (previous_container) {
        EXPECT_EQ(LayoutTreeBuilderTraversal::NextSibling(*previous_container),
                  container_pseudo);
      }
      previous_container = container_pseudo;

      auto* image_wrapper_pseudo = container_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionImageWrapper, document_transition_tag);

      auto* outgoing_image = image_wrapper_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionOutgoingImage, document_transition_tag);
      ASSERT_TRUE(outgoing_image);
      EXPECT_TRUE(outgoing_image->GetComputedStyle());
      EXPECT_TRUE(outgoing_image->GetLayoutObject());

      auto* incoming_image = image_wrapper_pseudo->GetPseudoElement(
          kPseudoIdPageTransitionIncomingImage, document_transition_tag);

      if (!has_incoming_image) {
        ASSERT_FALSE(incoming_image);
        continue;
      }

      ASSERT_TRUE(incoming_image);
      EXPECT_TRUE(incoming_image->GetComputedStyle());
      EXPECT_TRUE(incoming_image->GetLayoutObject());
    }
  }

 protected:
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(DocumentTransitionTest);

TEST_P(DocumentTransitionTest, TransitionObjectPersists) {
  auto* first_transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto* second_transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());

  EXPECT_TRUE(first_transition);
  EXPECT_EQ(GetState(first_transition), State::kIdle);
  EXPECT_TRUE(second_transition);
  EXPECT_EQ(first_transition, second_transition);
}

TEST_P(DocumentTransitionTest, TransitionPreparePromiseResolves) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();
  ASSERT_TRUE(transition);
  EXPECT_EQ(GetState(transition), State::kIdle);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester promise_tester(
      script_state, transition->captureAndHold(script_state, exception_state));

  EXPECT_EQ(GetState(transition), State::kCapturing);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  promise_tester.WaitUntilSettled();

  EXPECT_TRUE(promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kCaptured);
}

TEST_P(DocumentTransitionTest, AdditionalPrepareAbortsTransition) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester first_promise_tester(
      script_state, transition->captureAndHold(script_state, exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  EXPECT_FALSE(exception_state.HadException());
  transition->captureAndHold(script_state, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(GetState(transition), State::kIdle);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  first_promise_tester.WaitUntilSettled();

  EXPECT_TRUE(first_promise_tester.IsRejected());
  EXPECT_EQ(GetState(transition), State::kIdle);
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
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  transition->setElement(script_state, e1, "e1", nullptr, exception_state);
  transition->setElement(script_state, e3, "e3", nullptr, exception_state);
  transition->captureAndHold(script_state, exception_state);

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e3));

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_TRUE(ElementIsComposited("e3"));

  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

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
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  transition->setElement(script_state, e1, "e1", nullptr, exception_state);
  transition->setElement(script_state, e2, "e2", nullptr, exception_state);
  transition->setElement(script_state, e3, "e3", nullptr, exception_state);
  transition->captureAndHold(script_state, exception_state);

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e3));

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  // Since only the first element is contained, the rest should be cleared.
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_FALSE(ElementIsComposited("e3"));
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
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  // Set two of the elements to be shared.
  transition->setElement(script_state, e1, "e1", nullptr, exception_state);
  transition->setElement(script_state, e3, "e3", nullptr, exception_state);
  transition->captureAndHold(script_state, exception_state);

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e3));

  UpdateAllLifecyclePhasesAndFinishDirectives();

  // Set two different elements as shared.
  // Unset e3.
  transition->setElement(script_state, e3, AtomicString(), nullptr,
                         exception_state);
  // Set e2 to be the same tag as "e3".
  // TODO(vmpstr): We should be able to support new tags for entry transitions.
  transition->setElement(script_state, e2, "e3", nullptr, exception_state);
  transition->start(script_state, exception_state);

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));
}

TEST_P(DocumentTransitionTest, AdditionalPrepareAfterPreparedAbortsTransition) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester first_promise_tester(
      script_state, transition->captureAndHold(script_state, exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  first_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(first_promise_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kCaptured);

  EXPECT_FALSE(exception_state.HadException());
  transition->captureAndHold(script_state, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

TEST_P(DocumentTransitionTest, TransitionCleanedUpBeforePromiseResolution) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();
  ScriptPromiseTester tester(
      script_state, transition->captureAndHold(script_state, exception_state));

  // ActiveScriptWrappable should keep the transition alive.
  ThreadState::Current()->CollectAllGarbageForTesting();

  UpdateAllLifecyclePhasesAndFinishDirectives();
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_P(DocumentTransitionTest, StartHasNoEffectUnlessPrepared) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();
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

TEST_P(DocumentTransitionTest, StartAfterPrepare) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester capture_tester(
      script_state, transition->captureAndHold(script_state, exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  capture_tester.WaitUntilSettled();
  EXPECT_TRUE(capture_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kCaptured);

  ScriptPromiseTester start_tester(
      script_state, transition->start(script_state, exception_state));
  // Take the request.
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  // Subsequent starts should get an exception and cancel an existing
  // transition.
  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  // We will have a release request at this point.
  EXPECT_TRUE(transition->TakePendingRequest());

  start_request->TakeFinishedCallback().Run();
  FinishTransition();
  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsRejected());
}

TEST_P(DocumentTransitionTest, StartPromiseIsResolved) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester capture_tester(
      script_state, transition->captureAndHold(script_state, exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  // Visual updates are allows during capture phase.
  EXPECT_FALSE(LayerTreeHost()->IsDeferringCommits());

  UpdateAllLifecyclePhasesAndFinishDirectives();
  capture_tester.WaitUntilSettled();
  EXPECT_TRUE(capture_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kCaptured);

  // Visual updates are stalled between captured and start.
  EXPECT_TRUE(LayerTreeHost()->IsDeferringCommits());

  ScriptPromiseTester start_tester(
      script_state, transition->start(script_state, exception_state));

  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  FinishTransition();

  // Visual updates are restored on start.
  EXPECT_FALSE(LayerTreeHost()->IsDeferringCommits());

  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

TEST_P(DocumentTransitionTest, Abandon) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  ScriptPromiseTester capture_tester(
      script_state, transition->captureAndHold(script_state, exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  transition->abandon(script_state, exception_state);

  capture_tester.WaitUntilSettled();
  EXPECT_TRUE(capture_tester.IsRejected());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

// Checks that the pseudo element tree is correctly build for ::transition*
// pseudo elements.
TEST_P(DocumentTransitionTest, DocumentTransitionPseudoTree) {
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
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  auto scope = transition->CreateScriptMutationsAllowedScope();

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  transition->setElement(script_state, e1, "e1", nullptr, exception_state);
  transition->setElement(script_state, e2, "e2", nullptr, exception_state);
  transition->setElement(script_state, e3, "e3", nullptr, exception_state);
  transition->captureAndHold(script_state, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  UpdateAllLifecyclePhasesForTest();

  // The prepare phase should generate the pseudo tree.
  const Vector<AtomicString> document_transition_tags = {"root", "e1", "e2",
                                                         "e3"};
  ValidatePseudoElementTree(document_transition_tags, false);

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  SetHtmlInnerHTML(R"HTML(
    <style>
      div { width: 200px; height: 200px; contain: paint }
    </style>

    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");
  transition->start(script_state, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // The start phase should generate pseudo elements for rendering new live
  // content.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  ValidatePseudoElementTree(document_transition_tags, true);

  // Finish the animations which should remove the pseudo element tree.
  FinishTransition();
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_FALSE(GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdPageTransition));
}

}  // namespace blink
