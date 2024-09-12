// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_doublesequence.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/intersection_observer_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {

class IntersectionObserverTest : public SimTest,
                                 public testing::WithParamInterface<bool>,
                                 private ScopedIntersectionOptimizationForTest {
 public:
  IntersectionObserverTest()
      : ScopedIntersectionOptimizationForTest(GetParam()) {}

 protected:
  void TestScrollMargin(int scroll_margin,
                        bool is_intersecting,
                        double intersectionRatio) {
    WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(R"HTML(
    <style>
    #scroller { width: 100px; height: 100px; overflow: scroll; }
    #spacer { width: 50px; height: 110px; }
    #target { width: 50px; height: 50px; }
    </style>

    <div id=scroller>
      <div id=spacer></div>
      <div id=target></div>
    </div>
  )HTML");

    Compositor().BeginFrame();

    Element* target = GetDocument().getElementById(AtomicString("target"));
    ASSERT_TRUE(target);

    TestIntersectionObserverDelegate* scroll_margin_delegate =
        MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

    IntersectionObserver* scroll_margin_observer =
        MakeGarbageCollected<IntersectionObserver>(
            *scroll_margin_delegate,
            LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
            IntersectionObserver::Params{
                .margin = {Length::Fixed(10)},
                .scroll_margin = {Length::Fixed(scroll_margin)},
                .thresholds = {
                    std::numeric_limits<float>::min(),
                }});

    DummyExceptionStateForTesting exception_state;
    scroll_margin_observer->observe(target, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    Compositor().BeginFrame();
    test::RunPendingTasks();
    ASSERT_FALSE(Compositor().NeedsBeginFrame());

    EXPECT_EQ(scroll_margin_delegate->CallCount(), 1);
    EXPECT_EQ(scroll_margin_delegate->EntryCount(), 1);
    EXPECT_EQ(is_intersecting,
              scroll_margin_delegate->LastEntry()->isIntersecting());
    EXPECT_NEAR(intersectionRatio,
                scroll_margin_delegate->LastEntry()->intersectionRatio(),
                0.001);
  }

  void TestScrollMarginNested(int scroll_margin,
                              bool is_intersecting,
                              double intersectionRatio) {
    WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(R"HTML(
    <style>
    #scroller { width: 100px; height: 100px; overflow: scroll; }
    #scroller2 { width: 130px; height: 130px; overflow: scroll; }
    #spacer { width: 10px; height: 110px; }
    #target { width: 50px; height: 50px; }
    </style>

    <div id=scroller2>
      <div id=scroller>
        <div id=spacer></div>
        <div id=target></div>
      </div>
    </div>
  )HTML");

    Compositor().BeginFrame();

    Element* target = GetDocument().getElementById(AtomicString("target"));
    ASSERT_TRUE(target);

    TestIntersectionObserverDelegate* scroll_margin_delegate =
        MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

    IntersectionObserver* scroll_margin_observer =
        MakeGarbageCollected<IntersectionObserver>(
            *scroll_margin_delegate,
            LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
            IntersectionObserver::Params{
                .margin = {Length::Fixed(10)},
                .scroll_margin = {Length::Fixed(scroll_margin)},
                .thresholds = {std::numeric_limits<float>::min()}});

    DummyExceptionStateForTesting exception_state;
    scroll_margin_observer->observe(target, exception_state);
    ASSERT_FALSE(exception_state.HadException());

    Compositor().BeginFrame();
    test::RunPendingTasks();
    ASSERT_FALSE(Compositor().NeedsBeginFrame());

    EXPECT_EQ(scroll_margin_delegate->CallCount(), 1);
    EXPECT_EQ(scroll_margin_delegate->EntryCount(), 1);
    EXPECT_EQ(is_intersecting,
              scroll_margin_delegate->LastEntry()->isIntersecting());
    EXPECT_NEAR(intersectionRatio,
                scroll_margin_delegate->LastEntry()->intersectionRatio(),
                0.001);
  }

  void TestMinScrollDeltaToUpdateWithIntermediateClip() {
    Element* root = GetDocument().getElementById(AtomicString("root"));
    Element* target = GetDocument().getElementById(AtomicString("target"));
    LocalFrameView* frame_view = GetDocument().View();

    auto* observer_init = IntersectionObserverInit::Create();
    observer_init->setRoot(
        MakeGarbageCollected<V8UnionDocumentOrElement>(root));
    DummyExceptionStateForTesting exception_state;
    TestIntersectionObserverDelegate* observer_delegate =
        MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
    IntersectionObserver* observer = IntersectionObserver::Create(
        observer_init, *observer_delegate,
        LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    observer->observe(target, exception_state);
    ASSERT_FALSE(exception_state.HadException());
    const IntersectionObservation* observation =
        target->IntersectionObserverData()->GetObservationFor(*observer);
    EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kRequired,
              frame_view->GetIntersectionObservationStateForTesting());

    Compositor().BeginFrame();
    test::RunPendingTasks();
    EXPECT_EQ(observer_delegate->CallCount(), 1);
    EXPECT_EQ(observer_delegate->EntryCount(), 1);
    EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
    EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kNotNeeded,
              frame_view->GetIntersectionObservationStateForTesting());

    root->scrollTo(0, 50);
    EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
              frame_view->GetIntersectionObservationStateForTesting());
    Compositor().BeginFrame();
    test::RunPendingTasks();
    EXPECT_EQ(observer_delegate->CallCount(), 1);
    EXPECT_EQ(observer_delegate->EntryCount(), 1);
    EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());

    root->scrollTo(0, 100);
    EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
              frame_view->GetIntersectionObservationStateForTesting());
    Compositor().BeginFrame();
    test::RunPendingTasks();
    EXPECT_EQ(observer_delegate->CallCount(), 2);
    EXPECT_EQ(observer_delegate->EntryCount(), 2);
    EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
    EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kNotNeeded,
              frame_view->GetIntersectionObservationStateForTesting());

    root->scrollTo(0, 101);
    EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
              frame_view->GetIntersectionObservationStateForTesting());
    Compositor().BeginFrame();
    test::RunPendingTasks();
    EXPECT_EQ(observer_delegate->CallCount(), 2);
    EXPECT_EQ(observer_delegate->EntryCount(), 2);
    EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
    EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
    EXPECT_EQ(LocalFrameView::kNotNeeded,
              frame_view->GetIntersectionObservationStateForTesting());
  }

  bool CanUseCachedRects(const IntersectionObservation& observation) {
    return observation.CanUseCachedRectsForTesting(
        GetDocument().View()->GetIntersectionObservationStateForTesting() <=
        LocalFrameView::kScrollAndVisibilityOnly);
  }
};

class IntersectionObserverV2Test : public IntersectionObserverTest {
 public:
  IntersectionObserverV2Test() {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
  }

  ~IntersectionObserverV2Test() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
  }
};

INSTANTIATE_TEST_SUITE_P(All, IntersectionObserverTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, IntersectionObserverV2Test, testing::Bool());

TEST_P(IntersectionObserverTest, ObserveSchedulesFrame) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div id='target'></div>");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_TRUE(observer->takeRecords(exception_state).empty());
  EXPECT_EQ(observer_delegate->CallCount(), 0);

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

TEST_P(IntersectionObserverTest, NotificationSentWhenRootRemoved) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #target {
      width: 100px;
      height: 100px;
    }
    </style>
    <div id='root'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());

  root->remove();
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, DocumentRootClips) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <iframe src="iframe.html" style="width:200px; height:100px"></iframe>
  )HTML");
  iframe_resource.Complete(R"HTML(
    <div id='target'>Hello, world!</div>
    <div id='spacer' style='height:2000px'></div>
  )HTML");
  Compositor().BeginFrame();

  Document* iframe_document = To<WebLocalFrameImpl>(MainFrame().FirstChild())
                                  ->GetFrame()
                                  ->GetDocument();
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(
      MakeGarbageCollected<V8UnionDocumentOrElement>(iframe_document));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = iframe_document->getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());

  iframe_document->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, ReportsFractionOfTargetOrRoot) {
  // Place a 100x100 target element in the middle of a 200x200 main frame.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #target {
      position: absolute;
      top: 50px; left: 50px; width: 100px; height: 100px;
    }
    </style>
    <div id='target'></div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  // 100% of the target element's area intersects with the frame.
  constexpr float kExpectedFractionOfTarget = 1.0f;

  // 25% of the frame's area is covered by the target element.
  constexpr float kExpectedFractionOfRoot = 0.25f;

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {kExpectedFractionOfTarget / 2},
          });

  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  TestIntersectionObserverDelegate* root_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* root_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *root_observer_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {kExpectedFractionOfRoot / 2},
              .semantics = IntersectionObserver::kFractionOfRoot});

  root_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());
  EXPECT_NEAR(kExpectedFractionOfTarget,
              target_observer_delegate->LastEntry()->intersectionRatio(), 1e-6);

  EXPECT_EQ(root_observer_delegate->CallCount(), 1);
  EXPECT_EQ(root_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(root_observer_delegate->LastEntry()->isIntersecting());
  EXPECT_NEAR(kExpectedFractionOfRoot,
              root_observer_delegate->LastEntry()->intersectionRatio(), 1e-6);
}

TEST_P(IntersectionObserverTest, TargetRectIsEmptyAfterMapping) {
  // Place a 100x100 target element in the middle of a 200x200 main frame.
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    .clipper {
      transform: rotatey(90deg);
    }
    .container {
      overflow: hidden;
    }
    #target {
      width: 10px;
      height: 10px;
    }
    </style>
    <div class=clipper>
      <div class=container>
        <div id=target></div>
      </div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, DirectlyUpdateTransform) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body {
      width: 500px;
      height: 500px;
    }
    #container {
      transform: translateX(100px);
      width: 100px;
    }
    #target {
      width: 10px;
      height: 10px;
    }
    </style>
    <div id=container>
      <div id=target></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());

  Element* container = GetDocument().getElementById(AtomicString("container"));
  container->SetInlineStyleProperty(CSSPropertyID::kTransform,
                                    "translateX(300px)");
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(GetDocument().GetLayoutView()->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(
      GetDocument().GetLayoutView()->DescendantNeedsPaintPropertyUpdate());
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(LocalFrameView::kDesired,
            GetDocument().View()->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 2);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(target_observer_delegate->LastEntry()->isIntersecting());

  container->SetInlineStyleProperty(CSSPropertyID::kColor, "yellow");
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            GetDocument().View()->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 2);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(target_observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, VisibilityHiddenChangeSize) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body {
      width: 500px;
      height: 500px;
    }
    #target {
      position: absolute;
      visibility: hidden;
      top: -20px;
      width: 10px;
      height: 10px;
    }
    </style>
    <div id=target></div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* target_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_observer_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  target_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 1);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(target_observer_delegate->LastEntry()->isIntersecting());

  target->SetInlineStyleProperty(CSSPropertyID::kHeight, "100px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_observer_delegate->CallCount(), 2);
  EXPECT_EQ(target_observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(target_observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, ResumePostsTask) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // When document is not suspended, beginFrame() will generate notifications
  // and post a task to deliver them.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);

  // When a document is suspended, beginFrame() will generate a notification,
  // but it will not be delivered.  The notification will, however, be
  // available via takeRecords();
  WebView().GetPage()->SetPaused(true);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 0), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_FALSE(observer->takeRecords(exception_state).empty());

  // Generate a notification while document is suspended; then resume
  // document. Notification should happen in a post task.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  WebView().GetPage()->SetPaused(false);
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
}

TEST_P(IntersectionObserverTest, HitTestAfterMutation) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  GetDocument().View()->ScheduleAnimation();

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);

  HitTestLocation location{PhysicalOffset()};
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent),
      location);
  GetDocument().View()->GetLayoutView()->HitTest(location, result);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
}

TEST_P(IntersectionObserverTest, DisconnectClearsNotifications) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='leading-space' style='height: 700px;'></div>
    <div id='target'></div>
    <div id='trailing-space' style='height: 700px;'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  observer->observe(target, exception_state);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 1u);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);

  // If disconnect() is called while an observer has unsent notifications,
  // those notifications should be discarded.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 300), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  observer->disconnect();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
}

TEST_P(IntersectionObserverTest, RootIntersectionWithForceZeroLayoutHeight) {
  WebView().GetSettings()->SetForceZeroLayoutHeight(true);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      body {
        margin: 0;
        height: 2000px;
      }

      #target {
        width: 100px;
        height: 100px;
        position: absolute;
        top: 1000px;
        left: 200px;
      }
    </style>
    <div id='target'></div>
  )HTML");

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_TRUE(observer_delegate->LastIntersectionRect().IsEmpty());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_FALSE(observer_delegate->LastIntersectionRect().IsEmpty());
  EXPECT_EQ(gfx::RectF(200, 400, 100, 100),
            observer_delegate->LastIntersectionRect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 1200), mojom::blink::ScrollType::kProgrammatic);
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_TRUE(observer_delegate->LastIntersectionRect().IsEmpty());
}

TEST_P(IntersectionObserverTest, TrackedTargetBookkeeping) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    </style>
    <div id='target'></div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer1 = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);
  observer1->observe(target);
  IntersectionObserver* observer2 = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);
  observer2->observe(target);

  ElementIntersectionObserverData* target_data =
      target->IntersectionObserverData();
  ASSERT_TRUE(target_data);
  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 2u);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);

  target->remove();
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
  GetDocument().body()->AppendChild(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 2u);

  observer1->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 1u);

  observer2->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
}

TEST_P(IntersectionObserverTest, TrackedRootBookkeeping) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root'>
      <div id='target1'></div>
      <div id='target2'></div>
    </div>
  )HTML");

  IntersectionObserverController& controller =
      GetDocument().EnsureIntersectionObserverController();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  Persistent<Element> root = GetDocument().getElementById(AtomicString("root"));
  Persistent<Element> target =
      GetDocument().getElementById(AtomicString("target1"));
  Persistent<IntersectionObserverInit> observer_init =
      IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);

  // For an explicit-root observer, the root element is tracked only when it
  // has observations and is connected. Target elements are not tracked.
  ElementIntersectionObserverData* root_data = root->IntersectionObserverData();
  ASSERT_TRUE(root_data);
  EXPECT_FALSE(root_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  observer->observe(target);
  ElementIntersectionObserverData* target_data =
      target->IntersectionObserverData();
  ASSERT_TRUE(target_data);
  EXPECT_FALSE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // Root should not be tracked if it's not connected.
  root->remove();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  GetDocument().body()->AppendChild(root);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);

  // Root should not be tracked if it has no observations.
  observer->disconnect();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  observer->observe(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  observer->unobserve(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  observer->observe(target);
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);

  // The existing observation should keep the observer alive and active.
  // Flush any pending notifications, which hold a hard reference to the
  // observer and can prevent it from being gc'ed. The observation will be the
  // only thing keeping the observer alive.
  test::RunPendingTasks();
  observer_delegate->Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(root_data->IsEmpty());
  EXPECT_FALSE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 1u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // When the last observation is disconnected, as a result of the target
  // being gc'ed, the root element should no longer be tracked after the next
  // lifecycle update.
  target->remove();
  target = nullptr;
  target_data = nullptr;
  // Removing the target from the DOM tree forces a notification to be
  // queued, so flush it out.
  test::RunPendingTasks();
  observer_delegate->Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  Compositor().BeginFrame();
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);

  // Removing the last reference to the observer should allow it to be dropeed
  // from the root's ElementIntersectionObserverData.
  observer = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(root_data->IsEmpty());

  target = GetDocument().getElementById(AtomicString("target2"));
  observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);
  observer->observe(target);
  target_data = target->IntersectionObserverData();
  ASSERT_TRUE(target_data);

  // If the explicit root of an observer goes away, any existing observations
  // should be disconnected.
  target->remove();
  root->remove();
  root = nullptr;
  test::RunPendingTasks();
  observer_delegate->Clear();
  observer_delegate = nullptr;
  observer_init = nullptr;
  // Removing the target from the tree is not enough to disconnect the
  // observation.
  EXPECT_FALSE(target_data->IsEmpty());
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(target_data->IsEmpty());
  EXPECT_EQ(controller.GetTrackedObserverCountForTesting(), 0u);
  EXPECT_EQ(controller.GetTrackedObservationCountForTesting(), 0u);
}

TEST_P(IntersectionObserverTest, InaccessibleTarget) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id=target></div>
  )HTML");

  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer = IntersectionObserver::Create(
      IntersectionObserverInit::Create(), *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);

  Persistent<Element> target =
      GetDocument().getElementById(AtomicString("target"));
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  ASSERT_FALSE(observer->HasPendingActivity());

  // When we start observing a target, we should queue up a task to deliver
  // the observation. The observer should have pending activity.
  observer->observe(target);
  Compositor().BeginFrame();
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  EXPECT_TRUE(observer->HasPendingActivity());

  // After the observation is delivered, the observer no longer has activity
  // pending.
  test::RunPendingTasks();
  ASSERT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_FALSE(observer->HasPendingActivity());

  WeakPersistent<TestIntersectionObserverDelegate> observer_delegate_weak =
      observer_delegate.Get();
  WeakPersistent<IntersectionObserver> observer_weak = observer.Get();
  WeakPersistent<Element> target_weak = target.Get();
  ASSERT_TRUE(target_weak);
  ASSERT_TRUE(observer_weak);
  ASSERT_TRUE(observer_delegate_weak);

  // When |target| is no longer live, and |observer| has no more pending
  // tasks, both should be garbage-collected.
  target->remove();
  target = nullptr;
  observer = nullptr;
  observer_delegate = nullptr;
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target_weak);
  EXPECT_FALSE(observer_weak);
  EXPECT_FALSE(observer_delegate_weak);
}

TEST_P(IntersectionObserverTest, InaccessibleTargetBeforeDelivery) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id=target></div>
  )HTML");

  Persistent<TestIntersectionObserverDelegate> observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  Persistent<IntersectionObserver> observer = IntersectionObserver::Create(
      IntersectionObserverInit::Create(), *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver);

  Persistent<Element> target =
      GetDocument().getElementById(AtomicString("target"));
  ASSERT_EQ(observer_delegate->CallCount(), 0);
  ASSERT_FALSE(observer->HasPendingActivity());

  WeakPersistent<TestIntersectionObserverDelegate> observer_delegate_weak =
      observer_delegate.Get();
  WeakPersistent<IntersectionObserver> observer_weak = observer.Get();
  WeakPersistent<Element> target_weak = target.Get();
  ASSERT_TRUE(target_weak);
  ASSERT_TRUE(observer_weak);
  ASSERT_TRUE(observer_delegate_weak);

  // When we start observing |target|, a task should be queued to call the
  // callback with |target| and other information. So even if we remove
  // |target| in the same tick, |observer| would be kept alive.
  observer->observe(target);
  target->remove();
  target = nullptr;
  observer = nullptr;
  observer_delegate = nullptr;
  Compositor().BeginFrame();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(target_weak);
  EXPECT_TRUE(observer_weak);
  EXPECT_TRUE(observer_delegate_weak);

  // Once we run the callback, the observer has no more pending tasks, and so
  // it should be garbage-collected along with the target.
  test::RunPendingTasks();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(target_weak);
  EXPECT_FALSE(observer_weak);
  EXPECT_FALSE(observer_delegate_weak);
}

TEST_P(IntersectionObserverTest, RootMarginDevicePixelRatio) {
  WebView().SetZoomFactorForDeviceScaleFactor(3.5f);
  WebView().MainFrameViewWidget()->Resize(gfx::Size(2800, 2100));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body {
      margin: 0;
    }
    #target {
      height: 30px;
    }
    </style>
    <div id='target'>Hello, world!</div>
  )HTML");
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("-31px 0px 0px 0px");
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_RECTF_NEAR(observer_delegate->LastEntry()->GetGeometry().RootRect(),
                    gfx::RectF(0, 31, 800, 600 - 31), 0.0001);
}

TEST_P(IntersectionObserverTest, CachedRectsWithScrollers) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body { margin: 0; }
    .spacer { height: 1000px; }
    .scroller { overflow-y: scroll; height: 100px; position: relative; }
    </style>
    <div id='root' class='scroller'>
      <div id='target1-container'>
        <div id='target1'>Hello, world!</div>
      </div>
      <div class='scroller'>
        <div id='target2'>Hello, world!</div>
        <div class='spacer'></div>
      </div>
      <div class='scroller' style='overflow-y: hidden'>
        <div id='target3'>Hello, world!</div>
        <div class='spacer'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  Element* target3 = GetDocument().getElementById(AtomicString("target3"));
  // Ensure target3's ScrollTranslation node.
  target3->parentElement()->scrollTo(0, 10);

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target2, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target3, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // CanUseCachedRectsForTesting requires clean layout.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  IntersectionObservation* observation1 =
      target1->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  IntersectionObservation* observation2 =
      target2->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  IntersectionObservation* observation3 =
      target3->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Generate initial notifications and populate cache
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(CanUseCachedRects(*observation1));
  // observation2 can't use cached rects because the observer's root is not
  // the target's enclosing scroller.
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_FALSE(CanUseCachedRects(*observation3));
  } else {
    // This is incorrect.
    EXPECT_TRUE(CanUseCachedRects(*observation3));
  }

  // Scrolling the root should not invalidate.
  root->scrollTo(0, 100);
  target2->parentElement()->scrollTo(0, 100);
  target3->parentElement()->scrollTo(0, 100);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(CanUseCachedRects(*observation1));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_FALSE(CanUseCachedRects(*observation3));
  } else {
    // This is incorrect.
    EXPECT_TRUE(CanUseCachedRects(*observation3));
  }

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Scroll again.
  root->scrollTo(0, 200);
  target2->parentElement()->scrollTo(0, 200);
  target3->parentElement()->scrollTo(0, 200);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(CanUseCachedRects(*observation1));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_FALSE(CanUseCachedRects(*observation3));
  } else {
    // This is incorrect.
    EXPECT_TRUE(CanUseCachedRects(*observation3));
  }

  // Changing layout between root and target should invalidate.
  target1->parentElement()->SetInlineStyleProperty(CSSPropertyID::kMarginLeft,
                                                   "10px");
  // Invalidation happens during compositing inputs update, so force it here.
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Moving target2/target3 out from the subscroller should allow it to cache
  // rects.
  target2->remove();
  root->appendChild(target2);
  target3->remove();
  root->appendChild(target3);
  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(CanUseCachedRects(*observation1));
  EXPECT_TRUE(CanUseCachedRects(*observation2));
  EXPECT_TRUE(CanUseCachedRects(*observation3));
}

TEST_P(IntersectionObserverTest, CachedRectsWithOverflowHidden) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body { margin: 0; }
    .spacer { height: 1000px; }
    .scroller { overflow-y: hidden; height: 100px; position: relative; }
    </style>
    <div id='root' class='scroller'>
      <div id='target1-container'>
        <div id='target1'>Hello, world!</div>
      </div>
      <div class='scroller' style='overflow-y: scroll'>
        <div id='target2'>Hello, world!</div>
        <div class='spacer'></div>
      </div>
      <div class='scroller'>
        <div id='target3'>Hello, world!</div>
        <div class='spacer'></div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  Element* target3 = GetDocument().getElementById(AtomicString("target3"));

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target2, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target3, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // CanUseCachedRectsForTesting requires clean layout.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  IntersectionObservation* observation1 =
      target1->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  IntersectionObservation* observation2 =
      target2->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  IntersectionObservation* observation3 =
      target3->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Generate initial notifications and populate cache
  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation1));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation1));
  }
  // observation2 can't use cached rects because the observer's root is not
  // the target's enclosing scroller.
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Scrolling the root the first time creates a scroll translation node which
  // causes the invalidation.
  root->scrollTo(0, 100);
  target2->parentElement()->scrollTo(0, 100);
  target3->parentElement()->scrollTo(0, 100);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Generate initial notifications and populate cache
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Scroll again.
  root->scrollTo(0, 200);
  target2->parentElement()->scrollTo(0, 200);
  target3->parentElement()->scrollTo(0, 200);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation3));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Changing layout between root and target should invalidate.
  target1->parentElement()->SetInlineStyleProperty(CSSPropertyID::kMarginLeft,
                                                   "10px");
  // Invalidation happens during compositing inputs update, so force it here.
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation3));
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  EXPECT_FALSE(CanUseCachedRects(*observation3));

  // Moving target2/target3 out from the subscroller should allow it to cache
  // rects.
  target2->remove();
  root->appendChild(target2);
  target3->remove();
  root->appendChild(target3);
  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation1));
    EXPECT_TRUE(CanUseCachedRects(*observation2));
    EXPECT_TRUE(CanUseCachedRects(*observation3));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation1));
    EXPECT_FALSE(CanUseCachedRects(*observation2));
    EXPECT_FALSE(CanUseCachedRects(*observation3));
  }
}

TEST_P(IntersectionObserverTest, CachedRectsWithoutIntermediateScrollable) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    body { margin: 0; }
    .spacer { height: 1000px; }
    .scroller { overflow-y: scroll; height: 100px; }
    </style>
    <div id='scroller1' class='scroller'>
      <div id='root' style='position: absolute'>
        <div id='target1'>Target1</div>
        <div id='scroller2' class='scroller'>
          <div id='target2'>Target2</div>
          <!-- No spacer, thus this scroller is not scrollable. -->
        </div>
        <div id='scroller3' class='scroller'>
          <!-- target3 is not contained by the scroller -->
          <div id='target3' style='position: absolute'>Target3</div>
          <div id='target4'>Target4</div>
          <div class='spacer'></div>
        </div>
      </div>
      <div class='spacer'></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  Element* target3 = GetDocument().getElementById(AtomicString("target3"));
  Element* target4 = GetDocument().getElementById(AtomicString("target4"));

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target1, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target2, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target3, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target4, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // CanUseCachedRectsForTesting requires clean layout.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  IntersectionObservation* observation1 =
      target1->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  IntersectionObservation* observation2 =
      target2->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation2));
  IntersectionObservation* observation3 =
      target3->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation3));
  IntersectionObservation* observation4 =
      target4->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation4));

  // Generate initial notifications and populate cache.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation1));
    EXPECT_FALSE(CanUseCachedRects(*observation2));
    EXPECT_TRUE(CanUseCachedRects(*observation3));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation1));
    EXPECT_FALSE(CanUseCachedRects(*observation2));
    EXPECT_FALSE(CanUseCachedRects(*observation3));
  }
  // scroller3 is an intermediate scroller between root and target4.
  EXPECT_FALSE(CanUseCachedRects(*observation4));
}

TEST_P(IntersectionObserverTest, CachedRectsWithPaintPropertyChange) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="position: absolute">
      <div id="container" style="opacity: 0.5; transform: translateX(10px)">
        <div id="target">Target</div>
      </div>
    </div>
  </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* target = GetDocument().getElementById(AtomicString("target"));

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation));

  // Generate initial notifications and populate cache.
  Compositor().BeginFrame();
  test::RunPendingTasks();
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation));
  }

  // Change of opacity doesn't invalidate cached rects.
  container->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0.6");
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation));
  }
  container->SetInlineStyleProperty(CSSPropertyID::kTransform,
                                    "translateY(20px)");
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation));
}

TEST_P(IntersectionObserverTest, CachedRectsDisplayNone) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root'>
      <div id='target'>Hello, world!</div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);

  // Generate initial notifications and populate cache.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation));
  }

  target->setAttribute(html_names::kStyleAttr, AtomicString("display: none"));
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(CanUseCachedRects(*observation));
}

TEST_P(IntersectionObserverTest, CachedRectsWithFixedPosition) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="fixed" style="position: fixed">
      <div id="child">Child</div>
    </div>
  )HTML");

  Element* fixed = GetDocument().getElementById(AtomicString("fixed"));
  Element* child = GetDocument().getElementById(AtomicString("child"));

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(fixed, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(child, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // CanUseCachedRectsForTesting requires clean layout.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  IntersectionObservation* observation1 =
      fixed->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation1));
  IntersectionObservation* observation2 =
      child->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_FALSE(CanUseCachedRects(*observation2));

  // Generate initial notifications and populate cache
  Compositor().BeginFrame();
  test::RunPendingTasks();

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation1));
    EXPECT_TRUE(CanUseCachedRects(*observation2));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation1));
    EXPECT_FALSE(CanUseCachedRects(*observation2));
  }

  GetDocument().domWindow()->scrollTo(0, 100);
  GetDocument().View()->UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kTest);
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    EXPECT_TRUE(CanUseCachedRects(*observation1));
    EXPECT_TRUE(CanUseCachedRects(*observation2));
  } else {
    EXPECT_FALSE(CanUseCachedRects(*observation1));
    EXPECT_FALSE(CanUseCachedRects(*observation2));
  }
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateNotScrollable) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(IntersectionGeometry::kInfiniteScrollDelta,
            observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateNotScrollableToScrollable) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 200px; height: 200px; overflow: scroll">
      <div style="height: 150px"></div>
      <div id="target" style="width: 30px; height: 30px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(IntersectionGeometry::kInfiniteScrollDelta,
            observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->SetInlineStyleProperty(CSSPropertyID::kHeight, "130px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(30, 20), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->SetInlineStyleProperty(CSSPropertyID::kHeight, "200px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(IntersectionGeometry::kInfiniteScrollDelta,
            observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateInlineLayout) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 200px; height: 200px; overflow: scroll">
      <div id="spacer" style="height: 150px"></div>
      <span id="target">Target</span>
      <div style="height: 200px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* spacer = GetDocument().getElementById(AtomicString("spacer"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(50, observation->MinScrollDeltaToUpdate().y());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  spacer->SetInlineStyleProperty(CSSPropertyID::kHeight, "220px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(20, observation->MinScrollDeltaToUpdate().y());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  spacer->SetInlineStyleProperty(CSSPropertyID::kHeight, "100px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(100, observation->MinScrollDeltaToUpdate().y());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateThresholdZero) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 50);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());

  root->scrollTo(0, 30);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  // This checks we didn't do a full update. MinScrollDeltaToUpdate is
  // subtracted by abs(scroll-delta). If we did a full update,
  // MinScrollDeltaToUpdate would be recomputed to (50, 70).
  EXPECT_EQ(gfx::Vector2dF(50, 30), observation->MinScrollDeltaToUpdate());

  root->scrollTo(0, 100);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(51, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(1, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateWithPageZoom) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  GetDocument().GetFrame()->SetLayoutZoomFactor(2);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  // The test HTML is the same as MinScrollDeltaToUpdateThresholdZero.
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(100, 200), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  // Note that this CSSOM function uses CSS (unzoomed) coordinates.
  root->scrollTo(0, 50);
  // While our internal geometries are zoomed.
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(100, 100), observation->MinScrollDeltaToUpdate());

  root->scrollTo(0, 100);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(100, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 101);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(100, 2), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(51, 101);
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(2, 2), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateImplicitRoot) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>body { margin: 0; }</style>
    <div style="height: 400px"></div>
    <div id='target' style="width: 50px; height: 100px"></div>
    <div style="width: 1000px; height: 1000px"></div>
  )HTML");

  LocalDOMWindow& window = Window();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  window.scrollTo(0, 50);
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  window.scrollTo(0, 100);
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  window.scrollTo(0, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  window.scrollTo(51, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(1, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdZeroIntermediateClip) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id="clip" style="width: 50px; height: 20px; overflow: clip">
        <div id='target' style="width: 50px; height: 100px"></div>
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  TestMinScrollDeltaToUpdateWithIntermediateClip();
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdZeroIntermediateClipPath) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id="clip" style="width: 50px; height: 20px; clip-path: border-box">
        <div id='target' style="width: 50px; height: 100px"></div>
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  TestMinScrollDeltaToUpdateWithIntermediateClip();
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdZeroClipPathOnTarget) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px;
                              clip-path: rect(0 50px 20px 0)"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  TestMinScrollDeltaToUpdateWithIntermediateClip();
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateMinimumThreshold) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  observer_init->setThreshold(
      MakeGarbageCollected<V8UnionDoubleOrDoubleSequence>(
          IntersectionObserver::kMinimumThreshold));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 50);
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());

  root->scrollTo(0, 100);
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(51, 101);
  EXPECT_EQ(gfx::Vector2dF(50, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(1, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateThreshold0_5) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  observer_init->setThreshold(
      MakeGarbageCollected<V8UnionDoubleOrDoubleSequence>(0.5));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 50);
  EXPECT_EQ(gfx::Vector2dF(50, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());

  root->scrollTo(0, 100);
  EXPECT_EQ(gfx::Vector2dF(50, 50), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 101);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 151);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateThresholdOne) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 50px; height: 100px; margin-left: 30px">
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  observer_init->setThreshold(
      MakeGarbageCollected<V8UnionDoubleOrDoubleSequence>(1));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(20, 200), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 100);
  EXPECT_EQ(gfx::Vector2dF(20, 200), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(20, 100), observation->MinScrollDeltaToUpdate());

  root->scrollTo(0, 200);
  EXPECT_EQ(gfx::Vector2dF(20, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(20, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(20, 200);
  EXPECT_EQ(gfx::Vector2dF(20, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(10, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(31, 201);
  EXPECT_EQ(gfx::Vector2dF(10, 0), observation->MinScrollDeltaToUpdate());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(1, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateThresholdOneOfRoot) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='root' style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id='target' style="width: 100px; height: 150px; margin-left: 30px">
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* observer = MakeGarbageCollected<IntersectionObserver>(
      *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      IntersectionObserver::Params{
          .root = root,
          .thresholds = {1},
          .semantics = IntersectionObserver::kFractionOfRoot,
      });

  DummyExceptionStateForTesting exception_state;
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(30, 200), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(0, 100);
  EXPECT_EQ(gfx::Vector2dF(30, 200), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(30, 100), observation->MinScrollDeltaToUpdate());

  root->scrollTo(30, 200);
  EXPECT_EQ(gfx::Vector2dF(30, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kScrollAndVisibilityOnly,
            frame_view->GetIntersectionObservationStateForTesting());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(0, 0), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());

  root->scrollTo(31, 201);
  EXPECT_EQ(gfx::Vector2dF(0, 0), observation->MinScrollDeltaToUpdate());
  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_EQ(gfx::Vector2dF(1, 1), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest, MinScrollDeltaToUpdateThresholdFilterOnRoot) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 100px; height: 100px; overflow: scroll;
                          filter: blur(20px)">
      <div style="height: 200px"></div>
      <div id="target" style="width: 100px; height: 150px"></div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(gfx::Vector2dF(100, 100), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdFilterOnTarget) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div id="target" style="width: 100px; height: 150px; filter: blur(20px)">
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  TestIntersectionObserverDelegate* observer_delegate_js =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  TestIntersectionObserverDelegate* observer_delegate_display_lock =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  IntersectionObserver* observer_js = IntersectionObserver::Create(
      observer_init, *observer_delegate_js,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  IntersectionObserver* observer_display_lock = IntersectionObserver::Create(
      observer_init, *observer_delegate_display_lock,
      LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer_js->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer_display_lock->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation_js =
      target->IntersectionObserverData()->GetObservationFor(*observer_js);
  EXPECT_EQ(gfx::Vector2dF(), observation_js->MinScrollDeltaToUpdate());
  const IntersectionObservation* observation_display_lock =
      target->IntersectionObserverData()->GetObservationFor(
          *observer_display_lock);
  EXPECT_EQ(gfx::Vector2dF(),
            observation_display_lock->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(gfx::Vector2dF(100, 100), observation_js->MinScrollDeltaToUpdate());
  EXPECT_EQ(gfx::Vector2dF(),
            observation_display_lock->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdFilterOnIntermediateContainer) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 200px"></div>
      <div style="filter: blur(20px)">
        <div id="target" style="width: 100px; height: 150px"></div>
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverTest,
       MinScrollDeltaToUpdateThresholdFilterOnIntermediateNonContainer) {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    return;
  }
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id="root" style="width: 100px; height: 100px; overflow: scroll;
                          position: relative">
      <div style="height: 200px"></div>
      <div style="filter: blur(20px)">
        <div id="target" style="width: 100px; height: 150px;
                                position: absolute"></div>
      </div>
      <div style="width: 1000px; height: 1000px"></div>
    </div>
  )HTML");

  Element* root = GetDocument().getElementById(AtomicString("root"));
  Element* target = GetDocument().getElementById(AtomicString("target"));
  LocalFrameView* frame_view = GetDocument().View();

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kDisplayLockIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  const IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*observer);
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kRequired,
            frame_view->GetIntersectionObservationStateForTesting());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(gfx::Vector2dF(), observation->MinScrollDeltaToUpdate());
  EXPECT_EQ(LocalFrameView::kNotNeeded,
            frame_view->GetIntersectionObservationStateForTesting());
}

TEST_P(IntersectionObserverV2Test, TrackVisibilityInit) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(observer->trackVisibility());

  // This should fail because no delay is set.
  {
    DummyExceptionStateForTesting exception_state;
    observer_init->setTrackVisibility(true);
    observer = IntersectionObserver::Create(
        observer_init, *observer_delegate,
        LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
        exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  // This should fail because the delay is < 100.
  {
    DummyExceptionStateForTesting exception_state;
    observer_init->setDelay(99.9);
    observer = IntersectionObserver::Create(
        observer_init, *observer_delegate,
        LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
        exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    observer_init->setDelay(101.);
    observer = IntersectionObserver::Create(
        observer_init, *observer_delegate,
        LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    EXPECT_TRUE(observer->trackVisibility());
    EXPECT_EQ(observer->delay(), 101.);
  }
}

TEST_P(IntersectionObserverV2Test, BasicOcclusion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='target'>
      <div id='child'></div>
    </div>
    <div id='occluder'></div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* occluder = GetDocument().getElementById(AtomicString("occluder"));
  ASSERT_TRUE(target);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  occluder->SetInlineStyleProperty(CSSPropertyID::kMarginTop, "-10px");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());

  // Zero-opacity objects should not count as occluding.
  occluder->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 3);
  EXPECT_EQ(observer_delegate->EntryCount(), 3);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());
}

TEST_P(IntersectionObserverV2Test, BasicOpacity) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='transparent'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* transparent =
      GetDocument().getElementById(AtomicString("transparent"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(transparent);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  transparent->SetInlineStyleProperty(CSSPropertyID::kOpacity, "0.99");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());
}

TEST_P(IntersectionObserverV2Test, BasicTransform) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='transformed'>
      <div id='target'></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setTrackVisibility(true);
  observer_init->setDelay(100);
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  Element* transformed =
      GetDocument().getElementById(AtomicString("transformed"));
  ASSERT_TRUE(target);
  ASSERT_TRUE(transformed);
  observer->observe(target);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_TRUE(observer_delegate->LastEntry()->isVisible());

  // 2D translations and proportional upscaling is permitted.
  transformed->SetInlineStyleProperty(
      CSSPropertyID::kTransform, "translateX(10px) translateY(20px) scale(2)");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);

  // Any other transform is not permitted.
  transformed->SetInlineStyleProperty(CSSPropertyID::kTransform,
                                      "skewX(10deg)");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());
  EXPECT_EQ(observer_delegate->CallCount(), 2);
  EXPECT_EQ(observer_delegate->EntryCount(), 2);
  EXPECT_TRUE(observer_delegate->LastEntry()->isIntersecting());
  EXPECT_FALSE(observer_delegate->LastEntry()->isVisible());
}

TEST_P(IntersectionObserverTest, ApplyMarginToTarget) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #scroller { height: 100px; overflow: scroll; }
    #target { width: 50px; height: 50px; }
    .spacer { height: 105px; }
    </style>
    <div id=scroller>
      <div class=spacer></div>
      <div id=target></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* root_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* root_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *root_margin_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .margin = {Length::Fixed(10)},
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  root_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  TestIntersectionObserverDelegate* target_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  // Same parameters as above except that margin is applied to target.
  IntersectionObserver* target_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_margin_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .margin = {Length::Fixed(10)},
              .margin_target = IntersectionObserver::kApplyMarginToTarget,
              .thresholds = {std::numeric_limits<float>::min()},
          });

  target_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(root_margin_delegate->CallCount(), 1);
  EXPECT_EQ(root_margin_delegate->EntryCount(), 1);
  // Since the inner scroller clips content, the root margin has no effect and
  // target is not intersecting.
  EXPECT_FALSE(root_margin_delegate->LastEntry()->isIntersecting());

  EXPECT_EQ(target_margin_delegate->CallCount(), 1);
  EXPECT_EQ(target_margin_delegate->EntryCount(), 1);
  // Since the margin is applied to the target, the inner scroller clips an
  // expanded rect, which ends up being visible in the root. Hence, it is
  // intersecting.
  EXPECT_TRUE(target_margin_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, TargetMarginPercentResolvesAgainstRoot) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 500));
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #scroller { height: 100px; overflow: scroll; }
    #target { width: 50px; height: 50px; }
    .spacer { height: 145px; }
    </style>
    <div id=scroller>
      <div class=spacer></div>
      <div id=target></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  TestIntersectionObserverDelegate* target_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  // 10% margin on a target would be 5px if it resolved against target, which
  // is not enough to intersect. It would be 10px if it resolved against the
  // scroller, which is also not enough. However, it would be 50px if it
  // resolved against root, which would make it intersecting.
  IntersectionObserver* target_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *target_margin_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .margin = {Length::Percent(10)},
              .margin_target = IntersectionObserver::kApplyMarginToTarget,
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  target_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(target_margin_delegate->CallCount(), 1);
  EXPECT_EQ(target_margin_delegate->EntryCount(), 1);
  EXPECT_TRUE(target_margin_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, ScrollMarginIntersecting) {
  // The scroller should not clip the content because the scroll margin is
  // larger than the spacer and target should intersect.
  TestScrollMargin(/* scroll_margin */ 20, /* is_intersecting */ true,
                   /* intersectionRatio */ 0.2);
}

TEST_P(IntersectionObserverTest, ScrollMarginNotIntersecting) {
  // The scroller should clip the content because the scroll margin is smaller
  // than the spacer and target should not intersect.
  TestScrollMargin(/* scroll_margin */ 9, /* is_intersecting */ false,
                   /* intersectionRatio */ 0.0);
}

TEST_P(IntersectionObserverTest, NoScrollMargin) {
  // The scroller should clip the content because the scroll margin is zero
  // and target should not intersect.
  TestScrollMargin(/* scroll_margin */ 0, /* is_intersecting */ false,
                   /* intersectionRatio */ 0.0);
}

TEST_P(IntersectionObserverTest, ScrollMarginNestedIntersecting) {
  // The scroller should not clip the content because the scroll margin is
  // larger than the spacer and target should intersect.
  TestScrollMarginNested(/* scroll_margin */ 20, /* is_intersecting */ true,
                         /* intersectionRatio */ 0.2);
}

TEST_P(IntersectionObserverTest, ScrollMarginNestedNotIntersecting) {
  // The scroller should clip the content because the scroll margin is smaller
  // than the spacer and target should not intersect.
  TestScrollMarginNested(/* scroll_margin */ 9, /* is_intersecting */ false,
                         /* intersectionRatio */ 0.0);
}

TEST_P(IntersectionObserverTest, NoScrollMarginNested) {
  // The scroller should clip the content because the scroll margin is zero
  // and target should not intersect.
  TestScrollMarginNested(/* scroll_margin */ 0, /* is_intersecting */ false,
                         /* intersectionRatio */ 0.0);
}

TEST_P(IntersectionObserverTest, ScrollMarginIntersectingNonScrollingRoot) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(200, 200));

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <style>
    #scroller { width: 100px; height: 100px; overflow: scroll; }
    #spacer { width: 50px; height: 110px; }
    #root { height: 75; width: 75; }
    #target { width: 50px; height: 50px; }
    #spacer2 { width: 10px; height: 10px; }
    </style>

    <div id=scroller>
      <div id=spacer></div>
      <div id="root">
        <div class=spacer2></div>
        <div id=target></div>
      </div>
    </div>
  )HTML");

  Compositor().BeginFrame();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);

  Element* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);

  TestIntersectionObserverDelegate* scroll_margin_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* scroll_margin_observer =
      MakeGarbageCollected<IntersectionObserver>(
          *scroll_margin_delegate,
          LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
          IntersectionObserver::Params{
              .root = root,
              .margin = {Length::Fixed(10)},
              .thresholds = {std::numeric_limits<float>::min()},
          });

  DummyExceptionStateForTesting exception_state;
  scroll_margin_observer->observe(target, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  Compositor().BeginFrame();
  test::RunPendingTasks();
  ASSERT_FALSE(Compositor().NeedsBeginFrame());

  EXPECT_EQ(scroll_margin_delegate->CallCount(), 1);
  EXPECT_EQ(scroll_margin_delegate->EntryCount(), 1);
  EXPECT_TRUE(scroll_margin_delegate->LastEntry()->isIntersecting());
  EXPECT_NEAR(1.0, scroll_margin_delegate->LastEntry()->intersectionRatio(),
              0.001);
}

TEST_P(IntersectionObserverTest, InlineRoot) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <span id="root">
      <div id="target" style="display: inline-block">TARGET</div>
    </span>
  )HTML");
  Compositor().BeginFrame();

  Element* root = GetDocument().getElementById(AtomicString("root"));
  ASSERT_TRUE(root);
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRoot(MakeGarbageCollected<V8UnionDocumentOrElement>(root));
  DummyExceptionStateForTesting exception_state;
  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());
  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  observer->observe(target, exception_state);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  EXPECT_EQ(observer_delegate->CallCount(), 1);
  EXPECT_EQ(observer_delegate->EntryCount(), 1);
  // TODO(crbug.com/1456208): Support inline root.
  EXPECT_FALSE(observer_delegate->LastEntry()->isIntersecting());
}

TEST_P(IntersectionObserverTest, ParseMarginExtraText) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("1px 2px 3px 4px ExtraText");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Extra text found at the end of rootMargin.");
}

TEST_P(IntersectionObserverTest, ParseMarginUnsupportedUnitType) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("7x");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "rootMargin must be specified in pixels or percent.");
}

TEST_P(IntersectionObserverTest, ParseMarginUnsupportedUnit) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("7");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "rootMargin must be specified in pixels or percent.");
}

TEST_P(IntersectionObserverTest, RootMarginString) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("7px");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(observer->rootMargin(), "7px 7px 7px 7px");
}

TEST_P(IntersectionObserverTest, RootMarginPercentString) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setRootMargin("7%");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(observer->rootMargin(), "7% 7% 7% 7%");
}

TEST_P(IntersectionObserverTest, ScrollMarginEmptyString) {
  IntersectionObserverInit* observer_init = IntersectionObserverInit::Create();
  observer_init->setScrollMargin("");

  DummyExceptionStateForTesting exception_state;

  TestIntersectionObserverDelegate* observer_delegate =
      MakeGarbageCollected<TestIntersectionObserverDelegate>(GetDocument());

  IntersectionObserver* observer = IntersectionObserver::Create(
      observer_init, *observer_delegate,
      LocalFrameUkmAggregator::kJavascriptIntersectionObserver,
      exception_state);
  ASSERT_FALSE(exception_state.HadException());
  EXPECT_EQ(observer->scrollMargin(), "0px 0px 0px 0px");
}

}  // namespace blink
