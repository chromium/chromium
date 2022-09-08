// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include <memory>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "cc/document_transition/document_transition_request.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document_transition_callback.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_utils.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8-external.h"
#include "v8-function-callback.h"
#include "v8-function.h"
#include "v8-value.h"

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

  LayoutShiftTracker& GetLayoutShiftTracker() {
    return GetLocalFrameView()->GetLayoutShiftTracker();
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
           transition->NeedsSharedElementEffectNode(*layout_object);
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

TEST_P(DocumentTransitionTest, LayoutShift) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .shared {
        width: 100px;
        height: 100px;
        page-transition-tag: shared;
        contain: layout;
        background: green;
      }
    </style>
    <div id=target class=shared></div>
  )HTML");

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  // We should have a transition pseudo
  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdPageTransition);
  ASSERT_TRUE(transition_pseudo);
  auto* container_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdPageTransitionContainer, "shared");
  ASSERT_TRUE(container_pseudo);
  auto* container_box = To<LayoutBox>(container_pseudo->GetLayoutObject());
  EXPECT_EQ(LayoutSize(100, 100), container_box->Size());

  // Shared elements should not cause a layout shift.
  auto* target =
      To<LayoutBox>(GetDocument().getElementById("target")->GetLayoutObject());
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(LayoutSize(100, 100), target->Size());

  start_request->TakeFinishedCallback().Run();
  FinishTransition();
  start_tester.WaitUntilSettled();
}

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
  ASSERT_TRUE(transition->StartNewTransition());
  ASSERT_TRUE(transition);
  EXPECT_EQ(GetState(transition), State::kIdle);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester promise_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));

  EXPECT_EQ(GetState(transition), State::kCapturing);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  FinishTransition();
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST_P(DocumentTransitionTest, AdditionalStartAbortsTransition) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectNoCall());

  ScriptPromiseTester first_promise_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, document_transition_callback,
                    exception_state);
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
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      div { width: 100px; height: 100px; contain: paint }
      #e1 { page-transition-tag: e1; }
      #e3 { page-transition-tag: e3; }
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
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  transition->start(script_state, document_transition_callback,
                    exception_state);

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
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      #e1 { width: 100px; height: 100px; contain: paint }
    </style>
    <div id=e1 style="page-transition-tag: e1"></div>
    <div id=e2 style="page-transition-tag: e2"></div>
    <div id=e3 style="page-transition-tag: e3"></div>
  )HTML");

  auto* e1 = GetDocument().getElementById("e1");
  auto* e2 = GetDocument().getElementById("e2");
  auto* e3 = GetDocument().getElementById("e3");

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectNoCall());

  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  transition->start(script_state, document_transition_callback,
                    exception_state);

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_FALSE(ElementIsComposited("e3"));
}

TEST_P(DocumentTransitionTest, StartSharedElementsWantToBeComposited) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      div { contain: paint; width: 100px; height: 100px; background: blue; }
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
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  // Set two of the elements to be shared.
  e1->setAttribute(html_names::kStyleAttr, "page-transition-tag: e1");
  e3->setAttribute(html_names::kStyleAttr, "page-transition-tag: e3");

  struct Data {
    STACK_ALLOCATED();

   public:
    Data(Document& document,
         DocumentTransition* transition,
         ScriptState* script_state,
         ExceptionState& exception_state,
         Element* e1,
         Element* e2)
        : document(document),
          transition(transition),
          script_state(script_state),
          exception_state(exception_state),
          e1(e1),
          e2(e2) {}

    Document& document;
    DocumentTransition* transition;
    ScriptState* script_state;
    ExceptionState& exception_state;
    Element* e1;
    Element* e2;
  };
  Data data(GetDocument(), transition, script_state, exception_state, e1, e2);

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto* data =
            static_cast<Data*>(info.Data().As<v8::External>()->Value());
        data->document.getElementById("e1")->setAttribute(
            html_names::kStyleAttr, "");
        data->document.getElementById("e3")->setAttribute(
            html_names::kStyleAttr, "");
        data->e1->setAttribute(html_names::kStyleAttr,
                               "page-transition-tag: e1");
        data->e2->setAttribute(html_names::kStyleAttr,
                               "page-transition-tag: e2");
      };
  auto start_setup_callback =
      v8::Function::New(v8_scope.GetContext(), start_setup_lambda,
                        v8::External::New(v8_scope.GetIsolate(), &data))
          .ToLocalChecked();

  transition->start(script_state,
                    V8DocumentTransitionCallback::Create(start_setup_callback),
                    exception_state);

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e3));

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();

  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e1));
  EXPECT_TRUE(ShouldCompositeForDocumentTransition(e2));
  EXPECT_FALSE(ShouldCompositeForDocumentTransition(e3));
}

TEST_P(DocumentTransitionTest, AdditionalStartAfterStartAbortsTransition) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester first_promise_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, document_transition_callback,
                    exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

TEST_P(DocumentTransitionTest, TransitionCleanedUpBeforePromiseResolution) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());
  ASSERT_TRUE(transition);
  EXPECT_EQ(GetState(transition), State::kIdle);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester promise_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));

  // ActiveScriptWrappable should keep the transition alive.
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(GetState(transition), State::kCapturing);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  FinishTransition();
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST_P(DocumentTransitionTest, MultipleStartsBeforeTransitionFinished) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  auto start_request = transition->TakePendingRequest();
  EXPECT_TRUE(start_request);
  EXPECT_EQ(GetState(transition), State::kStarted);

  // Subsequent starts should get an exception and cancel an existing
  // transition.
  EXPECT_FALSE(exception_state.HadException());
  transition->start(script_state, document_transition_callback,
                    exception_state);
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
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectCall());

  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  UpdateAllLifecyclePhasesForTest();
  GetDocument().GetPage()->GetChromeClient().WillCommitCompositorFrame();

  // Visual updates paused during capture phase.
  EXPECT_TRUE(LayerTreeHost()->IsRenderingPaused());

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kCaptured);

  // Visual updates are stalled between captured and start.
  EXPECT_TRUE(LayerTreeHost()->IsRenderingPaused());

  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  FinishTransition();

  // Visual updates are restored on start.
  EXPECT_FALSE(LayerTreeHost()->IsRenderingPaused());

  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsFulfilled());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

TEST_P(DocumentTransitionTest, Abandon) {
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  MockFunctionScope funcs(script_state);
  auto* document_transition_callback =
      V8DocumentTransitionCallback::Create(funcs.ExpectNoCall());

  ScriptPromiseTester start_tester(
      script_state,
      transition->start(script_state, document_transition_callback,
                        exception_state));
  EXPECT_EQ(GetState(transition), State::kCapturing);

  transition->abandon(script_state, exception_state);

  start_tester.WaitUntilSettled();
  EXPECT_TRUE(start_tester.IsRejected());
  EXPECT_EQ(GetState(transition), State::kIdle);
}

// Checks that the pseudo element tree is correctly build for ::transition*
// pseudo elements.
TEST_P(DocumentTransitionTest, DocumentTransitionPseudoTree) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      div { width: 100px; height: 100px; contain: paint; background: blue }
    </style>

    <div id=e1 style="page-transition-tag: e1"></div>
    <div id=e2 style="page-transition-tag: e2"></div>
    <div id=e3 style="page-transition-tag: e3"></div>
  )HTML");

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  struct Data {
    STACK_ALLOCATED();

   public:
    Data(DocumentTransition* transition,
         ScriptState* script_state,
         ExceptionState& exception_state,
         Document& document)
        : transition(transition),
          script_state(script_state),
          exception_state(exception_state),
          document(document) {}

    DocumentTransition* transition;
    ScriptState* script_state;
    ExceptionState& exception_state;
    Document& document;
  };
  Data data(transition, script_state, exception_state, GetDocument());

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};
  auto start_setup_callback =
      v8::Function::New(v8_scope.GetContext(), start_setup_lambda,
                        v8::External::New(v8_scope.GetIsolate(), &data))
          .ToLocalChecked();

  transition->start(script_state,
                    V8DocumentTransitionCallback::Create(start_setup_callback),
                    exception_state);
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
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      div { width: 200px; height: 200px; contain: paint }
    </style>

    <div id=e1 style="page-transition-tag: e1"></div>
    <div id=e2 style="page-transition-tag: e2"></div>
    <div id=e3 style="page-transition-tag: e3"></div>
  )HTML");
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);

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

TEST_P(DocumentTransitionTest, DocumentTransitionSharedElementInvalidation) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      div {
        width: 100px;
        height: 100px;
        contain: paint;
        page-transition-tag: shared;
      }
    </style>

    <div id=element></div>
  )HTML");

  auto* element = GetDocument().getElementById("element");

  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(v8_scope.GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  transition->start(script_state,
                    V8DocumentTransitionCallback::Create(start_setup_callback),
                    exception_state);
  ASSERT_FALSE(exception_state.HadException());
  UpdateAllLifecyclePhasesForTest();

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);

  // The start phase should generate pseudo elements for rendering new live
  // content.
  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  // Finish the animations which should remove the pseudo element tree.
  FinishTransition();

  EXPECT_TRUE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesAndFinishDirectives();
}

TEST_P(DocumentTransitionTest, InspectorStyleResolver) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { page-transition-tag: root; }
      ::page-transition {
        background-color: red;
      }
      ::page-transition-container(foo) {
        background-color: blue;
      }
      ::page-transition-image-wrapper(foo) {
        background-color: lightblue;
      }
      ::page-transition-incoming-image(foo) {
        background-color: black;
      }
      ::page-transition-outgoing-image(foo) {
        background-color: grey;
      }
      div {
        page-transition-tag: foo;
        width: 100px;
        height: 100px;
        contain: paint;
      }
    </style>
    <div></div>
  )HTML");
  auto* transition =
      DocumentTransitionSupplement::EnsureDocumentTransition(GetDocument());
  ASSERT_TRUE(transition->StartNewTransition());

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  ExceptionState& exception_state = v8_scope.GetExceptionState();

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(v8_scope.GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  transition->start(script_state,
                    V8DocumentTransitionCallback::Create(start_setup_callback),
                    exception_state);
  ASSERT_FALSE(exception_state.HadException());
  UpdateAllLifecyclePhasesForTest();

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kStarted);

  struct TestCase {
    PseudoId pseudo_id;
    bool uses_tags;
    String user_rule;
  };
  TestCase test_cases[] = {
      {kPseudoIdPageTransition, false,
       "::page-transition { background-color: red; }"},
      {kPseudoIdPageTransitionContainer, true,
       "::page-transition-container(foo) { background-color: blue; }"},
      {kPseudoIdPageTransitionImageWrapper, true,
       "::page-transition-image-wrapper(foo) { background-color: lightblue; }"},
      {kPseudoIdPageTransitionIncomingImage, true,
       "::page-transition-incoming-image(foo) { background-color: black; }"},
      {kPseudoIdPageTransitionOutgoingImage, true,
       "::page-transition-outgoing-image(foo) { background-color: grey; }"}};

  for (const auto& test_case : test_cases) {
    InspectorStyleResolver resolver(GetDocument().documentElement(),
                                    test_case.pseudo_id,
                                    test_case.uses_tags ? "foo" : g_null_atom);
    auto* pseudo_element_rules = resolver.MatchedRules();

    // The resolver collects developer and UA rules.
    EXPECT_GT(pseudo_element_rules->size(), 1u);
    EXPECT_EQ(pseudo_element_rules->back().first->cssText(),
              test_case.user_rule);
  }

  InspectorStyleResolver parent_resolver(GetDocument().documentElement(),
                                         kPseudoIdNone, g_null_atom);
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(PseudoElementTagName(test_case.pseudo_id));
    Member<InspectorCSSMatchedRules> matched_rules_for_pseudo;

    bool found_rule_for_root = false;
    for (const auto& matched_rules : parent_resolver.PseudoElementRules()) {
      if (matched_rules->pseudo_id != test_case.pseudo_id)
        continue;
      if (matched_rules->document_transition_tag == "root") {
        EXPECT_FALSE(found_rule_for_root);
        found_rule_for_root = true;
        continue;
      }

      EXPECT_FALSE(matched_rules_for_pseudo);
      matched_rules_for_pseudo = matched_rules;
    }

    ASSERT_TRUE(matched_rules_for_pseudo);
    // Pseudo elements which are generated for each tag should include the root
    // by default.
    EXPECT_EQ(found_rule_for_root, test_case.uses_tags);
    EXPECT_EQ(matched_rules_for_pseudo->document_transition_tag,
              test_case.uses_tags ? "foo" : g_null_atom);

    auto pseudo_element_rules = matched_rules_for_pseudo->matched_rules;
    // The resolver collects developer and UA rules.
    EXPECT_GT(pseudo_element_rules->size(), 1u);
    EXPECT_EQ(pseudo_element_rules->back().first->cssText(),
              test_case.user_rule);
  }
}

}  // namespace blink
