// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition.h"

#include <memory>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "cc/view_transition/view_transition_request.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_transition_callback.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-value.h"

namespace blink {

class ViewTransitionTest : public testing::Test,
                           public PaintTestConfigurations,
                           private ScopedViewTransitionOnNavigationForTest {
 public:
  ViewTransitionTest() : ScopedViewTransitionOnNavigationForTest(true) {}

  void SetUp() override {
    web_view_helper_ = std::make_unique<frame_test_helpers::WebViewHelper>();
    web_view_helper_->Initialize();
    web_view_helper_->Resize(gfx::Size(200, 200));
    GetDocument().GetSettings()->SetPreferCompositingToLCDTextForTesting(true);
  }

  void TearDown() override { web_view_helper_.reset(); }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(
        web_view_helper_->GetWebView()->MainFrameImpl()->GetFrame());
  }

  Document& GetDocument() {
    return *web_view_helper_->GetWebView()
                ->MainFrameImpl()
                ->GetFrame()
                ->GetDocument();
  }

  bool ElementIsComposited(const char* id) {
    return !CcLayersByDOMElementId(RootCcLayer(), id).empty();
  }

  // Testing the compositor interaction is not in scope for these unittests. So,
  // instead of setting up a full commit flow, simulate it by calling the commit
  // callback directly.
  void UpdateAllLifecyclePhasesAndFinishDirectives() {
    UpdateAllLifecyclePhasesForTest();
    for (auto& callback :
         LayerTreeHost()->TakeViewTransitionCallbacksForTesting()) {
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

  using State = ViewTransition::State;

  State GetState(DOMViewTransition* transition) const {
    return transition->GetViewTransitionForTest()->state_;
  }

  void FinishTransition() {
    auto* transition = ViewTransitionUtils::GetTransition(GetDocument());
    if (transition)
      transition->SkipTransition();
  }

  bool ShouldCompositeForViewTransition(Element* e) {
    auto* layout_object = e->GetLayoutObject();
    auto* transition = ViewTransitionUtils::GetTransition(GetDocument());
    return layout_object && transition &&
           transition->NeedsViewTransitionEffectNode(*layout_object);
  }

  void ValidatePseudoElementTree(
      const Vector<WTF::AtomicString>& view_transition_names,
      bool has_incoming_image) {
    auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
        kPseudoIdViewTransition);
    ASSERT_TRUE(transition_pseudo);
    EXPECT_TRUE(transition_pseudo->GetComputedStyle());
    EXPECT_TRUE(transition_pseudo->GetLayoutObject());

    PseudoElement* previous_container = nullptr;
    for (const auto& view_transition_name : view_transition_names) {
      SCOPED_TRACE(view_transition_name);
      auto* container_pseudo = transition_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionGroup, view_transition_name);
      ASSERT_TRUE(container_pseudo);
      EXPECT_TRUE(container_pseudo->GetComputedStyle());
      EXPECT_TRUE(container_pseudo->GetLayoutObject());

      if (previous_container) {
        EXPECT_EQ(LayoutTreeBuilderTraversal::NextSibling(*previous_container),
                  container_pseudo);
      }
      previous_container = container_pseudo;

      auto* image_wrapper_pseudo = container_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionImagePair, view_transition_name);

      auto* outgoing_image = image_wrapper_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionOld, view_transition_name);
      ASSERT_TRUE(outgoing_image);
      EXPECT_TRUE(outgoing_image->GetComputedStyle());
      EXPECT_TRUE(outgoing_image->GetLayoutObject());

      auto* incoming_image = image_wrapper_pseudo->GetPseudoElement(
          kPseudoIdViewTransitionNew, view_transition_name);

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
  test::TaskEnvironment task_environment;

  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ViewTransitionTest);

TEST_P(ViewTransitionTest, LayoutShift) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .shared {
        width: 100px;
        height: 100px;
        view-transition-name: shared;
        contain: layout;
        background: green;
      }
    </style>
    <div id=target class=shared></div>
  )HTML");

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester finished_tester(script_state,
                                      transition->finished(script_state));
  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  // We should have a start request from the async callback passed to start()
  // resolving.
  test::RunPendingTasks();
  auto start_requests =
      ViewTransitionSupplement::From(GetDocument())->TakePendingRequests();
  EXPECT_FALSE(start_requests.empty());
  EXPECT_EQ(GetState(transition), State::kAnimating);

  // We should have a transition pseudo
  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition);
  ASSERT_TRUE(transition_pseudo);
  auto* container_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("shared"));
  ASSERT_TRUE(container_pseudo);
  auto* container_box = To<LayoutBox>(container_pseudo->GetLayoutObject());
  EXPECT_EQ(PhysicalSize(100, 100), container_box->Size());

  // View transition elements should not cause a layout shift.
  auto* target = To<LayoutBox>(
      GetDocument().getElementById(AtomicString("target"))->GetLayoutObject());
  EXPECT_FLOAT_EQ(0, GetLayoutShiftTracker().Score());
  EXPECT_EQ(PhysicalSize(100, 100), target->Size());

  FinishTransition();
  finished_tester.WaitUntilSettled();
}

TEST_P(ViewTransitionTest, TransitionCreatesNewObject) {
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* first_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());
  auto* second_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* first_transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), first_callback,
      IGNORE_EXCEPTION_FOR_TESTING);
  auto* second_transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), second_callback,
      IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_TRUE(first_transition);
  EXPECT_EQ(GetState(first_transition), State::kAborted);
  EXPECT_TRUE(second_transition);
  EXPECT_NE(first_transition, second_transition);

  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();
}

TEST_P(ViewTransitionTest, TransitionReadyPromiseResolves) {
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester promise_tester(script_state,
                                     transition->ready(script_state));

  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kAnimating);

  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  FinishTransition();
}

TEST_P(ViewTransitionTest, PrepareTransitionElementsWantToBeComposited) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      div { width: 100px; height: 100px; contain: paint }
      #e1 { view-transition-name: e1; }
      #e3 { view-transition-name: e3; }
    </style>

    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById(AtomicString("e1"));
  auto* e2 = GetDocument().getElementById(AtomicString("e2"));
  auto* e3 = GetDocument().getElementById(AtomicString("e3"));

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);
  EXPECT_FALSE(ShouldCompositeForViewTransition(e1));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e2));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e3));

  // Update the lifecycle while keeping the transition active.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetState(transition), State::kCapturing);
  EXPECT_TRUE(ShouldCompositeForViewTransition(e1));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e2));
  EXPECT_TRUE(ShouldCompositeForViewTransition(e3));

  EXPECT_TRUE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_TRUE(ElementIsComposited("e3"));

  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(ShouldCompositeForViewTransition(e1));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e2));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e3));

  // We need to actually run the lifecycle in order to see the full effect of
  // finishing directives.
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(ElementIsComposited("e1"));
  EXPECT_FALSE(ElementIsComposited("e2"));
  EXPECT_FALSE(ElementIsComposited("e3"));

  FinishTransition();
  test::RunPendingTasks();
}

TEST_P(ViewTransitionTest, StartTransitionElementsWantToBeComposited) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      div { contain: paint; width: 100px; height: 100px; background: blue; }
    </style>
    <div id=e1></div>
    <div id=e2></div>
    <div id=e3></div>
  )HTML");

  auto* e1 = GetDocument().getElementById(AtomicString("e1"));
  auto* e2 = GetDocument().getElementById(AtomicString("e2"));
  auto* e3 = GetDocument().getElementById(AtomicString("e3"));

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;

  // Set two of the elements to be shared.
  e1->setAttribute(html_names::kStyleAttr,
                   AtomicString("view-transition-name: e1"));
  e3->setAttribute(html_names::kStyleAttr,
                   AtomicString("view-transition-name: e3"));

  struct Data {
    STACK_ALLOCATED();

   public:
    Data(Document& document,
         ScriptState* script_state,
         ExceptionState& exception_state,
         Element* e1,
         Element* e2)
        : document(document),
          script_state(script_state),
          exception_state(exception_state),
          e1(e1),
          e2(e2) {}

    Document& document;
    ScriptState* script_state;
    ExceptionState& exception_state;
    Element* e1;
    Element* e2;
  };
  Data data(GetDocument(), script_state, exception_state, e1, e2);

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto* data =
            static_cast<Data*>(info.Data().As<v8::External>()->Value());
        data->document.getElementById(AtomicString("e1"))
            ->setAttribute(html_names::kStyleAttr, g_empty_atom);
        data->document.getElementById(AtomicString("e3"))
            ->setAttribute(html_names::kStyleAttr, g_empty_atom);
        data->e1->setAttribute(html_names::kStyleAttr,
                               AtomicString("view-transition-name: e1"));
        data->e2->setAttribute(html_names::kStyleAttr,
                               AtomicString("view-transition-name: e2"));
      };
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda,
                        v8::External::New(script_state->GetIsolate(), &data))
          .ToLocalChecked();

  ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback), exception_state);

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ShouldCompositeForViewTransition(e1));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e2));
  EXPECT_TRUE(ShouldCompositeForViewTransition(e3));

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();

  EXPECT_TRUE(ShouldCompositeForViewTransition(e1));
  EXPECT_TRUE(ShouldCompositeForViewTransition(e2));
  EXPECT_FALSE(ShouldCompositeForViewTransition(e3));

  FinishTransition();
  test::RunPendingTasks();
}

TEST_P(ViewTransitionTest, TransitionCleanedUpBeforePromiseResolution) {
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);
  ScriptPromiseTester promise_tester(script_state,
                                     transition->finished(script_state));

  // ActiveScriptWrappable should keep the transition alive.
  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);
  UpdateAllLifecyclePhasesAndFinishDirectives();
  FinishTransition();
  promise_tester.WaitUntilSettled();
  // There is no current way to successfully finish a transition from a
  // unittest. Web tests focus on successful completion tests.
  EXPECT_TRUE(promise_tester.IsFulfilled());
}

TEST_P(ViewTransitionTest, RenderingPausedTest) {
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);

  ScriptPromiseTester finished_tester(script_state,
                                      transition->finished(script_state));
  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);

  UpdateAllLifecyclePhasesForTest();
  GetDocument().GetPage()->GetChromeClient().WillCommitCompositorFrame();

  // Visual updates paused during capture phase.
  EXPECT_TRUE(LayerTreeHost()->IsRenderingPaused());

  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_EQ(GetState(transition), State::kDOMCallbackRunning);

  // Visual updates are stalled between captured and start.
  EXPECT_TRUE(LayerTreeHost()->IsRenderingPaused());

  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // Visual updates are restored on start.
  EXPECT_FALSE(LayerTreeHost()->IsRenderingPaused());

  FinishTransition();
  finished_tester.WaitUntilSettled();
  EXPECT_TRUE(finished_tester.IsFulfilled());
}

TEST_P(ViewTransitionTest, Abandon) {
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  MockFunctionScope funcs(script_state);
  auto* view_transition_callback =
      V8ViewTransitionCallback::Create(funcs.ExpectCall()->V8Function());

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(), view_transition_callback,
      IGNORE_EXCEPTION_FOR_TESTING);
  ScriptPromiseTester finished_tester(script_state,
                                      transition->finished(script_state));
  EXPECT_EQ(GetState(transition), State::kCaptureTagDiscovery);

  transition->skipTransition();
  test::RunPendingTasks();

  finished_tester.WaitUntilSettled();
  EXPECT_TRUE(finished_tester.IsFulfilled());
}

// Checks that the pseudo element tree is correctly build for ::transition*
// pseudo elements.
TEST_P(ViewTransitionTest, ViewTransitionPseudoTree) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      div { width: 100px; height: 100px; contain: paint; background: blue }
    </style>

    <div id=e1 style="view-transition-name: e1"></div>
    <div id=e2 style="view-transition-name: e2"></div>
    <div id=e3 style="view-transition-name: e3"></div>
  )HTML");

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;

  struct Data {
    STACK_ALLOCATED();

   public:
    Data(ScriptState* script_state,
         ExceptionState& exception_state,
         Document& document)
        : script_state(script_state),
          exception_state(exception_state),
          document(document) {}

    ScriptState* script_state;
    ExceptionState& exception_state;
    Document& document;
  };
  Data data(script_state, exception_state, GetDocument());

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda,
                        v8::External::New(script_state->GetIsolate(), &data))
          .ToLocalChecked();

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  // The prepare phase should generate the pseudo tree.
  const Vector<AtomicString> view_transition_names = {
      AtomicString("root"), AtomicString("e1"), AtomicString("e2"),
      AtomicString("e3")};
  ValidatePseudoElementTree(view_transition_names, false);

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      div { width: 200px; height: 200px; contain: paint }
    </style>

    <div id=e1 style="view-transition-name: e1"></div>
    <div id=e2 style="view-transition-name: e2"></div>
    <div id=e3 style="view-transition-name: e3"></div>
  )HTML");
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);

  // The start phase should generate pseudo elements for rendering new live
  // content.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  ValidatePseudoElementTree(view_transition_names, true);

  // Finish the animations which should remove the pseudo element tree.
  FinishTransition();
  UpdateAllLifecyclePhasesAndFinishDirectives();
  EXPECT_FALSE(GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition));
}

TEST_P(ViewTransitionTest, ViewTransitionElementInvalidation) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      div {
        width: 100px;
        height: 100px;
        contain: paint;
        view-transition-name: shared;
      }
    </style>

    <div id=element></div>
  )HTML");

  auto* element = GetDocument().getElementById(AtomicString("element"));

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);

  // The start phase should generate pseudo elements for rendering new live
  // content.
  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_FALSE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  // Finish the animations which should remove the pseudo element tree.
  FinishTransition();

  EXPECT_TRUE(element->GetLayoutObject()->NeedsPaintPropertyUpdate());

  UpdateAllLifecyclePhasesAndFinishDirectives();
}

TEST_P(ViewTransitionTest, InspectorStyleResolver) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
      ::view-transition {
        background-color: red;
      }
      ::view-transition-group(foo) {
        background-color: blue;
      }
      ::view-transition-image-pair(foo) {
        background-color: lightblue;
      }
      ::view-transition-new(foo) {
        background-color: black;
      }
      ::view-transition-old(foo) {
        background-color: grey;
      }
      div {
        view-transition-name: foo;
        width: 100px;
        height: 100px;
        contain: paint;
      }
    </style>
    <div></div>
  )HTML");

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);

  struct TestCase {
    PseudoId pseudo_id;
    bool uses_tags;
    String user_rule;
  };
  TestCase test_cases[] = {
      {kPseudoIdViewTransition, false,
       "::view-transition { background-color: red; }"},
      {kPseudoIdViewTransitionGroup, true,
       "::view-transition-group(foo) { background-color: blue; }"},
      {kPseudoIdViewTransitionImagePair, true,
       "::view-transition-image-pair(foo) { background-color: lightblue; }"},
      {kPseudoIdViewTransitionNew, true,
       "::view-transition-new(foo) { background-color: black; }"},
      {kPseudoIdViewTransitionOld, true,
       "::view-transition-old(foo) { background-color: grey; }"}};

  for (const auto& test_case : test_cases) {
    InspectorStyleResolver resolver(
        GetDocument().documentElement(), test_case.pseudo_id,
        test_case.uses_tags ? AtomicString("foo") : g_null_atom);
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
      if (matched_rules->view_transition_name == "root") {
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
    EXPECT_EQ(matched_rules_for_pseudo->view_transition_name,
              test_case.uses_tags ? AtomicString("foo") : g_null_atom);

    auto pseudo_element_rules = matched_rules_for_pseudo->matched_rules;
    // The resolver collects developer and UA rules.
    EXPECT_GT(pseudo_element_rules->size(), 1u);
    EXPECT_EQ(pseudo_element_rules->back().first->cssText(),
              test_case.user_rule);
  }

  FinishTransition();
  UpdateAllLifecyclePhasesAndFinishDirectives();
}

TEST_P(ViewTransitionTest, VirtualKeyboardDoesntAffectSnapshotSize) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .target {
        contain: layout;
        width: 100px;
        height: 100px;
        view-transition-name: target;
      }
    </style>
    <div class="target">
    </div>
  )HTML");

  // Simulate a content-resizing virtual keyboard appearing.
  const int kVirtualKeyboardHeight = 50;
  gfx::Size original_size = web_view_helper_->GetWebView()->Size();

  ASSERT_GT(original_size.height(), kVirtualKeyboardHeight);
  gfx::Size new_size = gfx::Size(
      original_size.width(), original_size.height() - kVirtualKeyboardHeight);

  web_view_helper_->Resize(new_size);
  web_view_helper_->LocalMainFrame()
      ->FrameWidgetImpl()
      ->SetVirtualKeyboardResizeHeightForTesting(kVirtualKeyboardHeight);

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  // The snapshot rect should not have been shrunk by the virtual keyboard, even
  // though it shrinks the WebView.
  EXPECT_EQ(transition->GetViewTransitionForTest()->GetSnapshotRootSize(),
            original_size);

  // The height of the ::view-transition should come from the snapshot root
  // rect.
  {
    auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
        kPseudoIdViewTransition);
    int height = To<LayoutBox>(transition_pseudo->GetLayoutObject())
                     ->GetPhysicalFragment(0)
                     ->Size()
                     .height.ToInt();
    EXPECT_EQ(height, original_size.height());
  }

  // Finish the prepare phase, mutate the DOM and start the animation.
  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  EXPECT_EQ(GetState(transition), State::kAnimating);

  // Simulate hiding the virtual keyboard.
  web_view_helper_->Resize(original_size);
  web_view_helper_->LocalMainFrame()
      ->FrameWidgetImpl()
      ->SetVirtualKeyboardResizeHeightForTesting(0);

  // The snapshot rect should remain the same size.
  EXPECT_EQ(transition->GetViewTransitionForTest()->GetSnapshotRootSize(),
            original_size);

  // The start phase should generate pseudo elements for rendering new live
  // content.
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // Finish the animations which should remove the pseudo element tree.
  FinishTransition();

  UpdateAllLifecyclePhasesAndFinishDirectives();
}

TEST_P(ViewTransitionTest, DocumentWithNoDocumentElementHasNullTransition) {
  auto* document =
      Document::CreateForTest(*GetDocument().GetExecutionContext());
  ASSERT_FALSE(document->documentElement());

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  DOMViewTransition* transition = ViewTransitionSupplement::startViewTransition(
      script_state, *document,
      V8ViewTransitionCallback::Create(start_setup_callback),
      IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_FALSE(transition);
}

TEST_P(ViewTransitionTest, RootEffectLifetime) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      /* TODO(crbug.com/1336462): html.css is parsed before runtime flags are enabled */
      html { view-transition-name: root; }
    </style>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto* transition = ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(
      transition->GetViewTransitionForTest()->NeedsViewTransitionEffectNode(
          *GetDocument().GetLayoutView()));
}

TEST_P(ViewTransitionTest, PseudoAwareChildTraversal) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      :root {
        view-transition-name: none;
      }
      :root.transitioned {
        view-transition-name: root;
      }
      #foo {
        view-transition-name: foo;
      }
      #bar {
        view-transition-name: bar;
      }
      .transitioned #bar {
        view-transition-name: none;
      }
    </style>
    <div id="foo"></div>
    <div id="bar"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto* document =
            static_cast<Document*>(info.Data().As<v8::External>()->Value());
        document->documentElement()->classList().Add(
            AtomicString("transitioned"));
      };

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(
          script_state->GetContext(), start_setup_lambda,
          v8::External::New(script_state->GetIsolate(), &GetDocument()))
          .ToLocalChecked();

  ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition);
  ASSERT_TRUE(transition_pseudo);

  EXPECT_EQ(GetDocument().documentElement()->PseudoAwareFirstChild(),
            static_cast<Node*>(GetDocument().head()));
  EXPECT_EQ(GetDocument().documentElement()->PseudoAwareLastChild(),
            transition_pseudo);

  // Root is last since it doesn't appear until encountered in the new view.
  auto* foo_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("foo"));
  auto* bar_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("bar"));
  auto* root_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("root"));

  EXPECT_EQ(transition_pseudo->PseudoAwareFirstChild(), foo_group_pseudo);
  EXPECT_EQ(transition_pseudo->PseudoAwareLastChild(), root_group_pseudo);

  auto* root_image_pair_pseudo = root_group_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("root"));
  auto* foo_image_pair_pseudo = foo_group_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("foo"));
  auto* bar_image_pair_pseudo = bar_group_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("bar"));

  EXPECT_EQ(foo_group_pseudo->PseudoAwareFirstChild(), foo_image_pair_pseudo);
  EXPECT_EQ(foo_group_pseudo->PseudoAwareLastChild(), foo_image_pair_pseudo);

  auto* foo_old_pseudo = foo_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("foo"));
  auto* foo_new_pseudo = foo_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("foo"));

  EXPECT_EQ(foo_image_pair_pseudo->PseudoAwareFirstChild(), foo_old_pseudo);
  EXPECT_EQ(foo_image_pair_pseudo->PseudoAwareLastChild(), foo_new_pseudo);

  auto* bar_old_pseudo = bar_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("bar"));
  EXPECT_EQ(bar_image_pair_pseudo->PseudoAwareFirstChild(), bar_old_pseudo);
  EXPECT_EQ(bar_image_pair_pseudo->PseudoAwareLastChild(), bar_old_pseudo);

  auto* root_new_pseudo = root_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("root"));
  EXPECT_EQ(root_image_pair_pseudo->PseudoAwareFirstChild(), root_new_pseudo);
  EXPECT_EQ(root_image_pair_pseudo->PseudoAwareLastChild(), root_new_pseudo);
}

TEST_P(ViewTransitionTest, PseudoAwareSiblingTraversal) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #foo {
        view-transition-name: foo;
      }
      #bar {
        view-transition-name: bar;
      }
    </style>
    <div id="foo"></div>
    <div id="bar"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  auto* transition_pseudo = GetDocument().documentElement()->GetPseudoElement(
      kPseudoIdViewTransition);
  ASSERT_TRUE(transition_pseudo);

  EXPECT_FALSE(transition_pseudo->PseudoAwareNextSibling());
  EXPECT_EQ(transition_pseudo->PseudoAwarePreviousSibling(),
            GetDocument().QuerySelector(AtomicString("body")));

  auto* foo_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("foo"));
  auto* bar_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("bar"));
  auto* root_group_pseudo = transition_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionGroup, AtomicString("root"));

  EXPECT_EQ(root_group_pseudo->PseudoAwareNextSibling(), foo_group_pseudo);
  EXPECT_EQ(root_group_pseudo->PseudoAwarePreviousSibling(), nullptr);

  EXPECT_EQ(foo_group_pseudo->PseudoAwareNextSibling(), bar_group_pseudo);
  EXPECT_EQ(foo_group_pseudo->PseudoAwarePreviousSibling(), root_group_pseudo);

  EXPECT_EQ(bar_group_pseudo->PseudoAwareNextSibling(), nullptr);
  EXPECT_EQ(bar_group_pseudo->PseudoAwarePreviousSibling(), foo_group_pseudo);

  auto* foo_image_pair_pseudo = foo_group_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("foo"));
  auto* bar_image_pair_pseudo = bar_group_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("bar"));

  EXPECT_FALSE(foo_image_pair_pseudo->PseudoAwareNextSibling());
  EXPECT_FALSE(foo_image_pair_pseudo->PseudoAwarePreviousSibling());
  EXPECT_FALSE(bar_image_pair_pseudo->PseudoAwareNextSibling());
  EXPECT_FALSE(bar_image_pair_pseudo->PseudoAwarePreviousSibling());

  auto* foo_old_pseudo = foo_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("foo"));
  auto* foo_new_pseudo = foo_image_pair_pseudo->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("foo"));

  EXPECT_EQ(foo_old_pseudo->PseudoAwareNextSibling(), foo_new_pseudo);
  EXPECT_EQ(foo_old_pseudo->PseudoAwarePreviousSibling(), nullptr);
  EXPECT_EQ(foo_new_pseudo->PseudoAwareNextSibling(), nullptr);
  EXPECT_EQ(foo_new_pseudo->PseudoAwarePreviousSibling(), foo_old_pseudo);
}

TEST_P(ViewTransitionTest, IncludingPseudoTraversal) {
  SetHtmlInnerHTML(R"HTML(
  <style>
    html { display: list-item; }
    html::marker {}
    html::before { content: ''}
    html::after { content: '' }
  </style>
  <div id="foo"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  Node* root = &GetDocument();
  Element* html = GetDocument().QuerySelector(AtomicString("html"));
  PseudoElement* vt = html->GetPseudoElement(kPseudoIdViewTransition);
  PseudoElement* vt_group =
      vt->GetPseudoElement(kPseudoIdViewTransitionGroup, AtomicString("root"));
  PseudoElement* vt_image_pair = vt_group->GetPseudoElement(
      kPseudoIdViewTransitionImagePair, AtomicString("root"));
  PseudoElement* vt_old = vt_image_pair->GetPseudoElement(
      kPseudoIdViewTransitionOld, AtomicString("root"));
  PseudoElement* vt_new = vt_image_pair->GetPseudoElement(
      kPseudoIdViewTransitionNew, AtomicString("root"));

  PseudoElement* marker = html->GetPseudoElement(kPseudoIdMarker);
  PseudoElement* before = html->GetPseudoElement(kPseudoIdBefore);
  PseudoElement* after = html->GetPseudoElement(kPseudoIdAfter);

  Element* head = GetDocument().QuerySelector(AtomicString("head"));
  Element* style = GetDocument().QuerySelector(AtomicString("style"));
  Element* body = GetDocument().QuerySelector(AtomicString("body"));
  Element* foo = GetDocument().QuerySelector(AtomicString("#foo"));

  HeapVector<Member<Node>> preorder_traversal = {
      root, html,  marker, before,   head,          body,   style,
      foo,  after, vt,     vt_group, vt_image_pair, vt_old, vt_new};

  HeapVector<Member<Node>> forward_traversal;
  for (Node* cur = preorder_traversal.front(); cur;
       cur = NodeTraversal::NextIncludingPseudo(*cur)) {
    // Simplify the test by ignoring whitespace.
    if (cur->IsTextNode()) {
      continue;
    }
    forward_traversal.push_back(cur);
  }
  EXPECT_EQ(preorder_traversal, forward_traversal);

  HeapVector<Member<Node>> backward_traversal;
  for (Node* cur = preorder_traversal.back(); cur;
       cur = NodeTraversal::PreviousIncludingPseudo(*cur)) {
    if (cur->IsTextNode()) {
      continue;
    }
    backward_traversal.push_back(cur);
  }

  preorder_traversal.Reverse();
  EXPECT_EQ(preorder_traversal, backward_traversal);
}

// This test was added because of a crash in getAnimations. The crash would
// occur because getAnimations attempts to sort the animations into compositing
// order. The comparator used uses tree order in some situations which requires
// pseudo elements to implement tree traversal methods. The crash occurred only
// on Android, probably due to differences in the std::sort implementation.
TEST_P(ViewTransitionTest, GetAnimationsCrashTest) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #a {
        view-transition-name: a;
      }
      #b {
        view-transition-name: b;
      }
      #c {
        view-transition-name: c;
      }
      #d {
        view-transition-name: d;
      }
      #e {
        view-transition-name: e;
      }
      #f {
        view-transition-name: f;
      }
    </style>
    <div id="a"></div>
    <div id="b"></div>
    <div id="c"></div>
    <div id="d"></div>
    <div id="e"></div>
    <div id="f"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  ViewTransitionSupplement::startViewTransition(
      script_state, GetDocument(),
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  // This test passes if getAnimations() doesn't crash while trying to sort the
  // view-transitions animations.
  ASSERT_GT(GetDocument().getAnimations().size(), 0ul);
}

TEST_P(ViewTransitionTest, ScriptCallAfterNavigationTransition) {
  GetDocument().domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::Create(KURL("http://test.com")));
  GetDocument()
      .domWindow()
      ->GetFrame()
      ->Loader()
      .SetIsNotOnInitialEmptyDocument();

  auto* current_item = MakeGarbageCollected<HistoryItem>();
  current_item->SetURL(KURL("http://test.com"));
  GetDocument().domWindow()->navigation()->UpdateCurrentEntryForTesting(
      *current_item);

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto page_swap_params = mojom::blink::PageSwapEventParams::New();
  page_swap_params->url = KURL("http://test.com");
  page_swap_params->navigation_type =
      mojom::blink::NavigationTypeForNavigationApi::kPush;
  ViewTransitionSupplement::SnapshotDocumentForNavigation(
      GetDocument(), blink::ViewTransitionToken(), std::move(page_swap_params),
      base::BindOnce([](const ViewTransitionState&) {}));

  ASSERT_TRUE(ViewTransitionSupplement::From(GetDocument())->GetTransition());

  bool callback_issued = false;

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto* callback_issued =
            static_cast<bool*>(info.Data().As<v8::External>()->Value());
        *callback_issued = true;
      };
  auto start_setup_callback =
      v8::Function::New(
          script_state->GetContext(), start_setup_lambda,
          v8::External::New(script_state->GetIsolate(), &callback_issued))
          .ToLocalChecked();
  DOMViewTransition* script_transition =
      ViewTransitionSupplement::startViewTransition(
          script_state, GetDocument(),
          V8ViewTransitionCallback::Create(start_setup_callback),
          IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_TRUE(script_transition);

  UpdateAllLifecyclePhasesAndFinishDirectives();
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesAndFinishDirectives();

  EXPECT_TRUE(callback_issued);
}

TEST_P(ViewTransitionTest, NoEffectOnIframe) {
  SetHtmlInnerHTML(R"HTML(
    <iframe id=frame srcdoc="<html></html>"></iframe>
  )HTML");
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto& child_document =
      *To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
           ->GetDocument();
  ViewTransitionSupplement::startViewTransition(
      script_state, child_document,
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();
  auto* paint_properties =
      child_document.GetLayoutView()->FirstFragment().PaintProperties();
  EXPECT_TRUE(!paint_properties || !paint_properties->Effect());
}

TEST_P(ViewTransitionTest, SubframeSnapshotLayer) {
  SetHtmlInnerHTML(R"HTML(
    <iframe id=frame srcdoc="<html></html>"></iframe>
  )HTML");
  test::RunPendingTasks();
  UpdateAllLifecyclePhasesForTest();

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  auto start_setup_lambda =
      [](const v8::FunctionCallbackInfo<v8::Value>& info) {};

  // This callback sets the elements for the start phase of the transition.
  auto start_setup_callback =
      v8::Function::New(script_state->GetContext(), start_setup_lambda, {})
          .ToLocalChecked();

  auto& child_document =
      *To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
           ->GetDocument();
  ViewTransitionSupplement::startViewTransition(
      script_state, child_document,
      V8ViewTransitionCallback::Create(start_setup_callback),
      ASSERT_NO_EXCEPTION);
  auto* transition = ViewTransitionUtils::GetTransition(child_document);
  ASSERT_TRUE(transition);

  UpdateAllLifecyclePhasesForTest();
  auto layer = transition->GetSubframeSnapshotLayer();
  ASSERT_TRUE(layer);
  EXPECT_TRUE(layer->is_live_content_layer_for_testing());

  child_document.GetPage()->GetChromeClient().WillCommitCompositorFrame();
  auto new_layer = transition->GetSubframeSnapshotLayer();
  ASSERT_TRUE(new_layer);
  EXPECT_NE(layer, new_layer);
  EXPECT_FALSE(new_layer->is_live_content_layer_for_testing());
}

}  // namespace blink
