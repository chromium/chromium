// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/trees/browser_controls_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class MockAnchorElementMetricsHost
    : public mojom::blink::AnchorElementMetricsHost {
 public:
  explicit MockAnchorElementMetricsHost(
      mojo::PendingReceiver<mojom::blink::AnchorElementMetricsHost>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

 private:
  // mojom::blink::AnchorElementMetricsHost:
  void ReportAnchorElementClick(
      mojom::blink::AnchorElementClickPtr click) override {
    clicks_.emplace_back(std::move(click));
  }

  void ReportAnchorElementsEnteredViewport(
      WTF::Vector<mojom::blink::AnchorElementEnteredViewportPtr> elements)
      override {
    for (auto& element : elements) {
      entered_viewport_.emplace_back(std::move(element));
    }
  }

  void ReportAnchorElementsLeftViewport(
      WTF::Vector<mojom::blink::AnchorElementLeftViewportPtr> elements)
      override {
    for (auto& element : elements) {
      left_viewport_.emplace_back(std::move(element));
    }
  }

  void ReportAnchorElementPointerDataOnHoverTimerFired(
      mojom::blink::AnchorElementPointerDataOnHoverTimerFiredPtr pointer_data)
      override {
    pointer_data_on_hover_.emplace_back(std::move(pointer_data));
  }

  void ReportAnchorElementPointerOver(
      mojom::blink::AnchorElementPointerOverPtr pointer_over_event) override {
    pointer_over_.emplace_back(std::move(pointer_over_event));
  }

  void ReportAnchorElementPointerOut(
      mojom::blink::AnchorElementPointerOutPtr hover_event) override {
    pointer_hover_dwell_time_.emplace_back(std::move(hover_event));
  }

  void ReportAnchorElementPointerDown(
      mojom::blink::AnchorElementPointerDownPtr pointer_down_event) override {
    pointer_down_.emplace_back(std::move(pointer_down_event));
  }

  void ReportAnchorElementsPositionUpdate(
      WTF::Vector<mojom::blink::AnchorElementPositionUpdatePtr>
          position_updates) override {
    for (auto& position_update : position_updates) {
      positions_[position_update->anchor_id] = std::move(position_update);
    }
  }

  void ReportNewAnchorElements(
      WTF::Vector<mojom::blink::AnchorElementMetricsPtr> elements,
      const WTF::Vector<uint32_t>& removed_elements) override {
    for (auto& element : elements) {
      auto [it, inserted] = anchor_ids_.insert(element->anchor_id);
      // Ignore duplicates.
      if (inserted) {
        elements_.emplace_back(std::move(element));
      }
    }
    removed_anchor_ids_.insert(removed_elements.begin(),
                               removed_elements.end());
  }

  void ProcessPointerEventUsingMLModel(
      mojom::blink::AnchorElementPointerEventForMLModelPtr pointer_event)
      override {}

  void ShouldSkipUpdateDelays(
      ShouldSkipUpdateDelaysCallback callback) override {
    // We don't use this mechanism to disable the delay of reports, as the tests
    // cover the delaying logic.
    std::move(callback).Run(false);
  }

 public:
  std::vector<mojom::blink::AnchorElementClickPtr> clicks_;
  std::vector<mojom::blink::AnchorElementEnteredViewportPtr> entered_viewport_;
  std::vector<mojom::blink::AnchorElementLeftViewportPtr> left_viewport_;
  std::map<uint32_t, mojom::blink::AnchorElementPositionUpdatePtr> positions_;
  std::vector<mojom::blink::AnchorElementPointerOverPtr> pointer_over_;
  std::vector<mojom::blink::AnchorElementPointerOutPtr>
      pointer_hover_dwell_time_;
  std::vector<mojom::blink::AnchorElementPointerDownPtr> pointer_down_;
  std::vector<mojom::blink::AnchorElementPointerDataOnHoverTimerFiredPtr>
      pointer_data_on_hover_;
  std::vector<mojom::blink::AnchorElementMetricsPtr> elements_;
  std::set<uint32_t> anchor_ids_;
  std::set<uint32_t> removed_anchor_ids_;

 private:
  mojo::Receiver<mojom::blink::AnchorElementMetricsHost> receiver_{this};
};

class TestWebFrameWidgetWithScreenInfo
    : public frame_test_helpers::TestWebFrameWidget {
 public:
  template <typename... Args>
  explicit TestWebFrameWidgetWithScreenInfo(
      display::ScreenInfo initial_screen_info,
      Args&&... args)
      : TestWebFrameWidget(std::forward<Args>(args)...),
        initial_screen_info_(initial_screen_info) {}

  display::ScreenInfo GetInitialScreenInfo() override {
    return initial_screen_info_;
  }

 private:
  display::ScreenInfo initial_screen_info_;
};

class AnchorElementMetricsSenderTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 600;

 protected:
  AnchorElementMetricsSenderTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    // Allows WidgetInputHandlerManager::InitOnInputHandlingThread() to run.
    platform_->RunForPeriod(base::Milliseconds(1));
    // Report all anchors to avoid non-deterministic behavior.
    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kNavigationPredictor, params);

    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);

    ResizeView(gfx::Size(kViewportWidth, kViewportHeight));
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);

    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementMetricsHost::Name_,
        WTF::BindRepeating(&AnchorElementMetricsSenderTest::Bind,
                           WTF::Unretained(this)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementMetricsHost::Name_, {});
    hosts_.clear();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    SimTest::TearDown();
  }

  frame_test_helpers::TestWebFrameWidget* CreateWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page) override {
    display::ScreenInfo screen_info;
    screen_info.rect = gfx::Rect(kViewportWidth, kViewportHeight);
    return MakeGarbageCollected<TestWebFrameWidgetWithScreenInfo>(
        screen_info, std::move(pass_key), std::move(frame_widget_host),
        std::move(frame_widget), std::move(widget_host), std::move(widget),
        std::move(task_runner), frame_sink_id, hidden, never_composited,
        is_for_child_local_root, is_for_nested_main_frame,
        is_for_scalable_page);
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    auto host = std::make_unique<MockAnchorElementMetricsHost>(
        mojo::PendingReceiver<mojom::blink::AnchorElementMetricsHost>(
            std::move(message_pipe_handle)));
    hosts_.push_back(std::move(host));
  }

  void ProcessEvents(size_t expected_anchors) {
    // Messages are buffered in the renderer and flushed after layout. However
    // since intersection observer detects elements that enter the viewport only
    // after layout, it takes two layout cycles for EnteredViewport messages to
    // be sent to the browser process.
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    // Fastforward execution of delayed tasks.
    if (auto* metrics_sender =
            AnchorElementMetricsSender::From(GetDocument())) {
      metrics_sender->FireUpdateTimerForTesting();
    }
    // Allow the mock host to process messages it received from the renderer.
    base::RunLoop().RunUntilIdle();
    // Wait until we've gotten the reports we expect.
    while (expected_anchors > 0 &&
           (hosts_.empty() || expected_anchors > hosts_[0]->elements_.size())) {
      // Wait 50ms.
      platform_->RunForPeriodSeconds(0.05);
      GetDocument().View()->UpdateAllLifecyclePhasesForTest();
      GetDocument().View()->UpdateAllLifecyclePhasesForTest();
      base::RunLoop().RunUntilIdle();
    }
  }

  void SetMockClock() {
    AnchorElementMetricsSender::From(GetDocument())
        ->SetTickClockForTesting(&clock_);
  }

  void VerticalScroll(float dy) {
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::BuildScrollBegin(
            /*dx_hint=*/0.0f, /*dy_hint=*/0.0f,
            WebGestureDevice::kTouchscreen));
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(
            /*dx=*/0.0f, dy, WebInputEvent::kNoModifiers,
            WebGestureDevice::kTouchscreen));
    GetWebFrameWidget().DispatchThroughCcInputHandler(
        SyntheticWebGestureEventBuilder::Build(
            WebInputEvent::Type::kGestureScrollEnd,
            WebGestureDevice::kTouchscreen));
    Compositor().BeginFrame();
  }

  void ProcessPositionUpdates() {
    platform_->RunForPeriodSeconds(ConvertDOMHighResTimeStampToSeconds(
        AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
            ->GetIntersectionObserverForTesting()
            ->delay()));
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    platform_->RunForPeriod(AnchorElementMetricsSender::kUpdateMetricsTimeGap);
    base::RunLoop().RunUntilIdle();
  }

  HTMLAnchorElement* AddAnchor(String inner_text, int height) {
    auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(GetDocument());
    anchor->setInnerText(inner_text);
    anchor->setHref("https://foo.com");
    anchor->SetInlineStyleProperty(CSSPropertyID::kHeight,
                                   String::Format("%dpx", height));
    anchor->SetInlineStyleProperty(CSSPropertyID::kDisplay, "block");
    GetDocument().body()->appendChild(anchor);
    return anchor;
  }

  base::test::ScopedFeatureList feature_list_;
  std::vector<std::unique_ptr<MockAnchorElementMetricsHost>> hosts_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  base::SimpleTestTickClock clock_;
};

// Test that anchors on non-HTTPS pages are not reported.
TEST_F(AnchorElementMetricsSenderTest, AddAnchorElementHTTP) {
  String source("http://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      R"HTML(<a id="anchor1" href="">example</a><a id="anchor2" href="">example</a>)HTML");

  ProcessEvents(0);
  EXPECT_EQ(0u, hosts_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AddAnchorElement) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      R"HTML(<a id="anchor1" href="">example</a><a id="anchor2" href="">example</a>)HTML");

  ProcessEvents(2);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(2u, mock_host->entered_viewport_.size());
  EXPECT_EQ(2u, mock_host->elements_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AddAnchorElementAfterLoad) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        // Add anchor 1s after onload.
        window.setTimeout(() => {
          const a = document.createElement('a');
          a.text = 'foo';
          a.href = '';
          document.body.appendChild(a);
          console.log('child appended');
        }, 1000);
      })
    </script>
  )HTML");

  // Wait until the script has had time to run.
  platform_->RunForPeriodSeconds(5.);
  ProcessEvents(1);

  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(mock_host->entered_viewport_[0]->anchor_id,
            mock_host->elements_[0]->anchor_id);
}

TEST_F(AnchorElementMetricsSenderTest, AddAndRemoveAnchorElement) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a1 = document.createElement('a');
        a1.text = 'foo';
        a1.href = '';
        document.body.appendChild(a1);
        a1.remove();
        const a2 = document.createElement('a');
        a2.text = 'bar';
        a2.href = '';
        document.body.appendChild(a2);
      });
    </script>
  )HTML");

  // `a1` was added and immediately removed, so it shouldn't be included.
  ProcessEvents(1);

  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  // Treat `a1` as if it were never added.
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AddAnchorElementFromDocumentFragment) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const fragment = new DocumentFragment();
        const a = document.createElement('a');
        a.text = 'foo';
        a.href = '';
        fragment.appendChild(a);
        document.body.appendChild(fragment);
      });
    </script>
  )HTML");

  ProcessEvents(1);

  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  // `a` was removed from the DocumentFragment in order to insert it into the
  // document, so it should not be considered removed.
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AnchorElementNeverConnected) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a1 = document.createElement('a');
        a1.text = 'a1';
        a1.href = '';
        const div = document.createElement('div');
        div.appendChild(a1);

        const a2 = document.createElement('a');
        a2.text = 'a2';
        a2.href = '';
        document.body.appendChild(a2);
      });
    </script>
  )HTML");

  // `a1` should not be processed.
  ProcessEvents(1);

  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
}

TEST_F(AnchorElementMetricsSenderTest, RemoveAnchorElement) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a1 = document.createElement('a');
        a1.text = 'foo';
        a1.href = '';
        document.body.appendChild(a1);
        window.a1 = a1;
      });
    </script>
  )HTML");

  // Initially, `a1` should be reported.
  ProcessEvents(1);
  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  ASSERT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
  uint32_t a1_id = mock_host->elements_[0]->anchor_id;

  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    window.a1.remove();
    const a2 = document.createElement('a');
    a2.text = 'bar';
    a2.href = '';
    document.body.appendChild(a2);
  )SCRIPT")
      ->RunScript(&Window());

  // For the next step, `a2` should be reported and `a1` should be reported as
  // removed.
  ProcessEvents(2);
  EXPECT_EQ(2u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->removed_anchor_ids_.size());
  EXPECT_TRUE(mock_host->removed_anchor_ids_.contains(a1_id));
}

TEST_F(AnchorElementMetricsSenderTest,
       RemoveAnchorElementWithoutMoreInsertions) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a1 = document.createElement('a');
        a1.text = 'foo';
        a1.href = '';
        document.body.appendChild(a1);
        window.a1 = a1;
      });
    </script>
  )HTML");

  ProcessEvents(1);
  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  ASSERT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
  uint32_t a1_id = mock_host->elements_[0]->anchor_id;

  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    window.a1.remove();
  )SCRIPT")
      ->RunScript(&Window());

  // We should have a report of just the removal of `a1`.
  ProcessEvents(1);
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->removed_anchor_ids_.size());
  EXPECT_TRUE(mock_host->removed_anchor_ids_.contains(a1_id));
}

TEST_F(AnchorElementMetricsSenderTest, RemoveMultipleParents) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a1grandparent = document.createElement('div');
        const a1parent = document.createElement('div');
        const a1 = document.createElement('a');
        a1.text = 'a1';
        a1.href = '';
        a1parent.appendChild(a1);
        a1grandparent.appendChild(a1parent);
        document.body.appendChild(a1grandparent);

        const a2grandparent = document.createElement('div');
        const a2parent = document.createElement('div');
        const a2 = document.createElement('a');
        a2.text = 'a2';
        a2.href = '';
        a2parent.appendChild(a2);
        a2grandparent.appendChild(a2parent);
        document.body.appendChild(a2grandparent);

        window.a1 = a1;
        window.a2 = a2;
      });
    </script>
  )HTML");

  ProcessEvents(2);
  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  ASSERT_EQ(2u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());

  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    window.a1.parentNode.parentNode.remove();
    window.a1.parentNode.remove();
    window.a1.remove();

    const a2grandparent = window.a2.parentNode.parentNode;
    const a2parent = window.a2.parentNode;
    const a2 = window.a2;
    a2grandparent.remove();
    a2parent.remove();
    a2.remove();
    a2parent.appendChild(a2);
    a2grandparent.appendChild(a2parent);
    document.body.appendChild(a2grandparent);

    const a3grandparent = document.createElement('div');
    const a3parent = document.createElement('div');
    const a3 = document.createElement('a');
    a3.text = 'a3';
    a3.href = '';
    a3parent.appendChild(a3);
    a3grandparent.appendChild(a3parent);
    document.body.appendChild(a3grandparent);
    a3grandparent.remove();
    a3parent.remove();
    a3.remove();

    const a4 = document.createElement('a');
    a4.text = 'a4';
    a4.href = '';
    document.body.appendChild(a4);
  )SCRIPT")
      ->RunScript(&Window());

  ProcessEvents(3);
  EXPECT_EQ(3u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->removed_anchor_ids_.size());
}

TEST_F(AnchorElementMetricsSenderTest, RemoveAnchorElementAfterLayout) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <script>
      window.addEventListener('load', () => {
        const a0 = document.createElement('a');
        a0.text = 'a0';
        a0.href = '';
        document.body.appendChild(a0);
        window.a0 = a0;
      });
    </script>
  )HTML");

  // Report an initial anchor.
  ProcessEvents(1);
  ASSERT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());

  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    const a1 = document.createElement('a');
    a1.text = 'a1';
    a1.href = '';
    document.body.appendChild(a1);

    const a2 = document.createElement('a');
    a2.text = 'a2';
    a2.href = '';
    document.body.appendChild(a2);

    const a3 = document.createElement('a');
    a3.text = 'a3';
    a3.href = '';
    document.body.appendChild(a3);

    const a4 = document.createElement('a');
    a4.text = 'a4';
    a4.href = '';
    document.body.appendChild(a4);

    const a5 = document.createElement('a');
    a5.text = 'a5';
    a5.href = '';
    document.body.appendChild(a5);

    window.a1 = a1;
    window.a2 = a2;
    window.a3 = a3;
    window.a4 = a4;
    window.a5 = a5;
  )SCRIPT")
      ->RunScript(&Window());

  // Layout so the metrics are buffered.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Before metrics are flushed, remove the initial anchor and `a1`, remove and
  // reinsert `a2`, repeatedly remove and reinsert `a3`, repeatedly remove and
  // reinsert then remove `a4`, remove `a5`, add a new anchor `a6`.
  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    window.a0.remove();
    window.a1.remove();

    window.a2.remove();
    document.body.appendChild(window.a2);

    window.a3.remove();
    document.body.appendChild(window.a3);
    window.a3.remove();
    document.body.appendChild(window.a3);

    window.a4.remove();
    document.body.appendChild(window.a4);
    window.a4.remove();

    window.a5.remove();

    const a6 = document.createElement('a');
    a6.text = 'a6';
    a6.href = '';
    document.body.appendChild(a6);
  )SCRIPT")
      ->RunScript(&Window());

  // After another buffering of metrics, reinsert `a5`.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  ClassicScript::CreateUnspecifiedScript(R"SCRIPT(
    document.body.appendChild(window.a5);
  )SCRIPT")
      ->RunScript(&Window());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Flush metrics.
  // At this point, 4 of the anchors are newly inserted and still inserted, 1
  // was previously reported and removed, 2 were newly inserted but removed
  // before the flush (so they're not reported).
  ProcessEvents(5);
  EXPECT_EQ(5u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->removed_anchor_ids_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AnchorElementLeftViewport) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(
      R"HTML(
        <body style="margin: 0px">
        <div style="height: %dpx;"></div>
        <a href="" style="width: 300px; height: %dpx;">foo</a>
        </body>)HTML",
      2 * kViewportHeight, kViewportHeight / 2));

  // Check that the element is registered, but there are no other events.
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(0u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());

  SetMockClock();
  AnchorElementMetricsSender::From(GetDocument())
      ->SetNowAsNavigationStartForTesting();

  // Scroll down. Now the anchor element is visible, and should report the
  // entered viewport event. |navigation_start_to_entered_viewport| should be
  // |wait_time1|.
  const auto wait_time1 = base::Milliseconds(100);
  clock_.Advance(wait_time1);

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 2 * kViewportHeight),
      mojom::blink::ScrollType::kProgrammatic);
  ProcessEvents(1);
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(
      wait_time1,
      mock_host->entered_viewport_[0]->navigation_start_to_entered_viewport);
  EXPECT_EQ(0u, mock_host->left_viewport_.size());

  // Scroll up. It should be out of view again, and should report the left
  // viewport event. |time_in_viewport| should be |time_in_viewport_1|.
  const auto time_in_viewport_1 = base::Milliseconds(150);
  clock_.Advance(time_in_viewport_1);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, -2 * kViewportHeight),
      mojom::blink::ScrollType::kProgrammatic);
  ProcessEvents(1);
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(1u, mock_host->left_viewport_.size());
  EXPECT_EQ(time_in_viewport_1, mock_host->left_viewport_[0]->time_in_viewport);

  // Scroll down to make it visible again. It should send a second entered
  // viewport event. |navigation_start_to_entered_viewport| should be
  // |wait_time1+time_in_viewport_1+wait_time2|.
  const auto wait_time2 = base::Milliseconds(100);
  clock_.Advance(wait_time2);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 2 * kViewportHeight),
      mojom::blink::ScrollType::kProgrammatic);
  ProcessEvents(1);
  EXPECT_EQ(2u, mock_host->entered_viewport_.size());
  EXPECT_EQ(
      wait_time1 + time_in_viewport_1 + wait_time2,
      mock_host->entered_viewport_[1]->navigation_start_to_entered_viewport);
  EXPECT_EQ(1u, mock_host->left_viewport_.size());

  // Scroll up to push it out of view again. It should send a second left
  // viewport event, and |time_in_viewport| should be |time_in_viewport_2|.
  const auto time_in_viewport_2 = base::Milliseconds(30);
  clock_.Advance(time_in_viewport_2);
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, -2 * kViewportHeight),
      mojom::blink::ScrollType::kProgrammatic);
  ProcessEvents(1);
  EXPECT_EQ(2u, mock_host->entered_viewport_.size());
  EXPECT_EQ(2u, mock_host->left_viewport_.size());
  EXPECT_EQ(time_in_viewport_2, mock_host->left_viewport_[1]->time_in_viewport);
}

TEST_F(AnchorElementMetricsSenderTest,
       AnchorElementInteractionTrackerSendsPointerEvents) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(
      R"HTML(
        <body style="margin: 0px">
        <a href="" style="width: %dpx; height: %dpx;">foo</a>
        </body>)HTML",
      kViewportWidth, kViewportHeight / 2));

  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());
  EXPECT_EQ(0u, mock_host->pointer_over_.size());
  EXPECT_EQ(0u, mock_host->pointer_hover_dwell_time_.size());

  auto move_to = [this](const auto x, const auto y) {
    gfx::PointF coordinates(x, y);
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates,
                        coordinates, WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
  };
  using Button = WebPointerProperties::Button;
  auto mouse_press = [this](const auto x, const auto y, const auto button) {
    gfx::PointF coordinates(x, y);
    WebInputEvent::Modifiers modifier = WebInputEvent::kLeftButtonDown;
    if (button == Button::kMiddle) {
      modifier = WebInputEvent::kMiddleButtonDown;
    } else if (button == Button::kMiddle) {
      modifier = WebInputEvent::kRightButtonDown;
    }
    WebMouseEvent event(WebInputEvent::Type::kMouseDown, coordinates,
                        coordinates, button, 0, modifier,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  };

  SetMockClock();
  AnchorElementMetricsSender::From(GetDocument())
      ->SetNowAsNavigationStartForTesting();
  // Move the pointer over the link for the first time. We should send pointer
  // over event. |navigation_start_to_pointer_over| should be |wait_time_1|.
  const auto wait_time_1 = base::Milliseconds(150);
  clock_.Advance(wait_time_1);
  move_to(0, 0);
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->pointer_over_.size());
  EXPECT_EQ(mock_host->elements_[0]->anchor_id,
            mock_host->pointer_over_[0]->anchor_id);
  EXPECT_EQ(wait_time_1,
            mock_host->pointer_over_[0]->navigation_start_to_pointer_over);
  EXPECT_EQ(0u, mock_host->pointer_hover_dwell_time_.size());

  // Move the pointer away. We should send pointer hover event and
  // |hover_dwell_time| should be |hover_dwell_time_1|.
  const auto hover_dwell_time_1 = base::Milliseconds(250);
  clock_.Advance(hover_dwell_time_1);
  move_to(kViewportWidth / 2, kViewportHeight);
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(1u, mock_host->pointer_over_.size());
  EXPECT_EQ(1u, mock_host->pointer_hover_dwell_time_.size());
  EXPECT_EQ(mock_host->elements_[0]->anchor_id,
            mock_host->pointer_hover_dwell_time_[0]->anchor_id);
  EXPECT_EQ(hover_dwell_time_1,
            mock_host->pointer_hover_dwell_time_[0]->hover_dwell_time);

  // Move the pointer over the link for a second time. We should send pointer
  // over event. |navigation_start_to_pointer_over| should be
  // |wait_time_1+hover_dwell_time_1+wait_time_2|.
  const auto wait_time_2 = base::Milliseconds(50);
  clock_.Advance(wait_time_2);
  move_to(0, 0);
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(2u, mock_host->pointer_over_.size());
  EXPECT_EQ(mock_host->elements_[0]->anchor_id,
            mock_host->pointer_over_[1]->anchor_id);
  EXPECT_EQ(wait_time_1 + hover_dwell_time_1 + wait_time_2,
            mock_host->pointer_over_[1]->navigation_start_to_pointer_over);
  EXPECT_EQ(1u, mock_host->pointer_hover_dwell_time_.size());

  // Move the pointer away for a second time. We should send pointer hover event
  // and |hover_dwell_time| should be |hover_dwell_time_2|.
  const auto hover_dwell_time_2 = base::Milliseconds(200);
  clock_.Advance(hover_dwell_time_2);
  move_to(kViewportWidth / 2, kViewportHeight);
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(2u, mock_host->pointer_over_.size());
  EXPECT_EQ(2u, mock_host->pointer_hover_dwell_time_.size());
  EXPECT_EQ(mock_host->elements_[0]->anchor_id,
            mock_host->pointer_hover_dwell_time_[1]->anchor_id);
  EXPECT_EQ(hover_dwell_time_2,
            mock_host->pointer_hover_dwell_time_[1]->hover_dwell_time);

  // Check mouse right button down event.
  move_to(0, 0);
  mouse_press(0, 0, /*button=*/Button::kRight);
  ProcessEvents(1);
  EXPECT_EQ(0u, mock_host->pointer_down_.size());

  // Check mouse left button down event.
  move_to(0, 0);
  mouse_press(0, 0, /*button=*/Button::kLeft);
  ProcessEvents(1);
  EXPECT_EQ(1u, mock_host->pointer_down_.size());
  EXPECT_EQ(wait_time_1 + hover_dwell_time_1 + wait_time_2 + hover_dwell_time_2,
            mock_host->pointer_down_[0]->navigation_start_to_pointer_down);

  // Check mouse middle button down event.
  move_to(0, 0);
  mouse_press(0, 0, /*button=*/Button::kMiddle);
  ProcessEvents(1);
  EXPECT_EQ(2u, mock_host->pointer_down_.size());
  EXPECT_EQ(wait_time_1 + hover_dwell_time_1 + wait_time_2 + hover_dwell_time_2,
            mock_host->pointer_down_[1]->navigation_start_to_pointer_down);
}

TEST_F(AnchorElementMetricsSenderTest, AnchorElementEnteredViewportLater) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(
      R"HTML(
        <body style="margin: 0px">
        <div style="height: %dpx;"></div>
        <a href="" style="width: 300px; height: %dpx;">foo</a>
        </body>)HTML",
      2 * kViewportHeight, kViewportHeight / 2));

  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(0u, mock_host->entered_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());

  // Scroll down. Now the anchor element is visible.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 2 * kViewportHeight),
      mojom::blink::ScrollType::kProgrammatic);
  ProcessEvents(1);
  EXPECT_EQ(1u, hosts_.size());
  EXPECT_EQ(0u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(mock_host->entered_viewport_[0]->anchor_id,
            mock_host->elements_[0]->anchor_id);
}

TEST_F(AnchorElementMetricsSenderTest, AnchorElementClicked) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  SimRequest next_page("https://example.com/p2", "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id="anchor" href="p2">foo</a>
    <script>
      window.addEventListener('load', () => {
        const a = document.getElementById('anchor');
        a.click();
      });
    </script>
  )HTML");

  ProcessEvents(0);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->clicks_.size());
  EXPECT_EQ("https://example.com/p2", mock_host->clicks_[0]->target_url);
  EXPECT_LE(base::TimeDelta(),
            mock_host->clicks_[0]->navigation_start_to_click);
  // Wait until the script has had time to run.
  platform_->RunForPeriodSeconds(5.);
  next_page.Complete("empty");
  ProcessEvents(0);
  // The second page load has no anchor elements and therefore no host is bound.
  ASSERT_EQ(1u, hosts_.size());
  EXPECT_EQ(1u, mock_host->clicks_.size());
}

TEST_F(AnchorElementMetricsSenderTest,
       ReportAnchorElementPointerDataOnHoverTimerFired) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a href='https://anchor1.com/'>
      <div style='padding: 0px; width: 400px; height: 400px;'></div>
    </a>
  )HTML");

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  GetDocument().GetAnchorElementInteractionTracker()->SetTaskRunnerForTesting(
      task_runner, task_runner->GetMockTickClock());

  constexpr gfx::PointF origin{200, 200};
  constexpr gfx::Vector2dF velocity{20, 20};
  constexpr base::TimeDelta timestep = base::Milliseconds(20);
  for (base::TimeDelta t;
       t <= 2 * AnchorElementInteractionTracker::GetHoverDwellTime();
       t += timestep) {
    gfx::PointF coordinates =
        origin + gfx::ScaleVector2d(velocity, t.InSecondsF());
    WebMouseEvent event(WebInputEvent::Type::kMouseMove, coordinates,
                        coordinates, WebPointerProperties::Button::kNoButton, 0,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
        event, Vector<WebMouseEvent>(), Vector<WebMouseEvent>());
    task_runner->AdvanceTimeAndRun(timestep);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->pointer_data_on_hover_.size());
  EXPECT_TRUE(
      mock_host->pointer_data_on_hover_[0]->pointer_data->is_mouse_pointer);
  EXPECT_NEAR(
      20.0 * std::sqrt(2.0),
      mock_host->pointer_data_on_hover_[0]->pointer_data->mouse_velocity, 0.5);
}

TEST_F(AnchorElementMetricsSenderTest, MaxIntersectionObservations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNavigationPredictor, {{"max_intersection_observations", "3"},
                                       {"random_anchor_sampling_period", "1"}});

  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"html(
    <body></body>
  )html");

  // Add 3 anchors; they should all be observed by the IntersectionObserver.
  auto* anchor_1 = AddAnchor("one", 100);
  auto* anchor_2 = AddAnchor("two", 200);
  auto* anchor_3 = AddAnchor("three", 300);

  ProcessEvents(3);
  ASSERT_EQ(1u, hosts_.size());
  auto* intersection_observer =
      AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
          ->GetIntersectionObserverForTesting();
  EXPECT_EQ(hosts_[0]->elements_.size(), 3u);
  EXPECT_EQ(intersection_observer->Observations().size(), 3u);

  auto observations = [&]() -> HeapVector<Member<HTMLAnchorElement>> {
    HeapVector<Member<HTMLAnchorElement>> observed_anchors;
    base::ranges::transform(
        intersection_observer->Observations(),
        std::back_inserter(observed_anchors),
        [](IntersectionObservation* observation) {
          return To<HTMLAnchorElement>(observation->Target());
        });
    return observed_anchors;
  };
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_1, anchor_2, anchor_3));

  // Remove anchor 1.
  anchor_1->remove();
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_2, anchor_3));

  // Readd anchor 1.
  GetDocument().body()->appendChild(anchor_1);
  ProcessEvents(3);
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_1, anchor_2, anchor_3));

  // Add a fourth anchor (larger than all existing anchors). It should be
  // observed instead of anchor 1.
  auto* anchor_4 = AddAnchor("four", 400);
  ProcessEvents(4);
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_2, anchor_3, anchor_4));

  // Add a fifth anchor (smaller than all existing anchors). The observations
  // should not change (i.e. it should not be observed).
  auto* anchor_5 = AddAnchor("five", 50);
  ProcessEvents(5);
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_2, anchor_3, anchor_4));

  // Remove anchor 2. It should no longer be observed, and anchor_1 (the
  // largest unobserved anchor) should be observed in its place.
  anchor_2->remove();
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_1, anchor_3, anchor_4));

  // Remove anchor 5. There should be no changes in anchors observed.
  anchor_5->remove();
  EXPECT_THAT(observations(),
              ::testing::UnorderedElementsAre(anchor_1, anchor_3, anchor_4));
}

TEST_F(AnchorElementMetricsSenderTest, AnchorUnobservedByIntersectionObserver) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNavigationPredictor, {{"max_intersection_observations", "1"},
                                       {"random_anchor_sampling_period", "1"}});

  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"html(
    <body></body>
  )html");

  auto* intersection_observer =
      AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
          ->GetIntersectionObserverForTesting();

  auto* anchor_1 = AddAnchor("one", 100);
  ProcessEvents(1);
  ASSERT_EQ(1u, hosts_.size());
  auto* host = hosts_[0].get();

  EXPECT_EQ(host->elements_.size(), 1u);
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  EXPECT_EQ(host->entered_viewport_.size(), 1u);

  host->entered_viewport_.clear();
  auto* anchor_2 = AddAnchor("two", 200);
  ProcessEvents(2);

  // `anchor_2` will now be observed by the intersection observer, `anchor_1`
  // will be unobserved, and should be reported as leaving the viewport.
  EXPECT_EQ(host->elements_.size(), 2u);
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  EXPECT_EQ(host->entered_viewport_.size(), 1u);
  ASSERT_EQ(host->left_viewport_.size(), 1u);
  EXPECT_EQ(AnchorElementId(*anchor_1), host->left_viewport_[0]->anchor_id);

  host->entered_viewport_.clear();
  host->left_viewport_.clear();
  AddAnchor("three", 50);
  ProcessEvents(3);

  // `anchor_3` will not be observed immediately by the intersection observer
  // (as it is smaller than anchor_2). No viewport messages should be
  // dispatched.
  EXPECT_EQ(host->elements_.size(), 3u);
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  EXPECT_EQ(host->entered_viewport_.size(), 0u);
  EXPECT_EQ(host->left_viewport_.size(), 0u);

  anchor_2->remove();
  ProcessEvents(2);

  // Note: We don't dispatch a "left viewport" message for anchor_2 here
  // because it was removed from the document; we just report it as a
  // removed anchor.
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  ASSERT_EQ(host->entered_viewport_.size(), 1u);
  EXPECT_EQ(AnchorElementId(*anchor_1), host->entered_viewport_[0]->anchor_id);
  EXPECT_EQ(host->left_viewport_.size(), 0u);
  EXPECT_EQ(host->removed_anchor_ids_.size(), 1u);
}

TEST_F(AnchorElementMetricsSenderTest,
       AnchorNotInViewportUnobservedByIntersectionObserver) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNavigationPredictor, {{"max_intersection_observations", "1"},
                                       {"random_anchor_sampling_period", "1"}});

  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(R"html(
    <body>
      <div style="height: %dpx;"></div>
    </body>
  )html",
                                        kViewportHeight + 100));

  AddAnchor("one", 100);
  ProcessEvents(1);
  ASSERT_EQ(1u, hosts_.size());
  auto* host = hosts_[0].get();
  auto* intersection_observer =
      AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
          ->GetIntersectionObserverForTesting();

  EXPECT_EQ(host->elements_.size(), 1u);
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  EXPECT_EQ(host->entered_viewport_.size(), 0u);
  EXPECT_EQ(host->left_viewport_.size(), 0u);

  AddAnchor("two", 200);
  ProcessEvents(2);

  // We don't dispatch "left viewport" for anchor_1 here, because it was
  // never reported to be in the viewport.
  EXPECT_EQ(intersection_observer->Observations().size(), 1u);
  EXPECT_EQ(host->entered_viewport_.size(), 0u);
  EXPECT_EQ(host->left_viewport_.size(), 0u);
}

TEST_F(AnchorElementMetricsSenderTest, IntersectionObserverDelay) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNavigationPredictor,
      {{"intersection_observer_delay", "252ms"}});

  String source("https://foo.com/bar.html");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete("");

  IntersectionObserver* intersection_observer =
      AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(GetDocument())
          ->GetIntersectionObserverForTesting();
  EXPECT_EQ(intersection_observer->delay(), 252.0);
}

TEST_F(AnchorElementMetricsSenderTest, PositionUpdate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNavigationPredictorNewViewportFeatures);
  String source("https://foo.com");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);

  // viewport |  div_1
  //    ..    |  div_1
  //    ..    |  div_1
  //    ..    |  anchor_1
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2
  // --------------------
  //   XXXX   |  anchor_2
  //   XXXX   |  anchor_3
  ASSERT_EQ(kViewportHeight % 8, 0);
  const int unit = kViewportHeight / 8;
  const int div_1_height = 3 * unit;
  const int anchor_1_height = 1 * unit;
  const int div_2_height = 4 * unit;
  const int anchor_2_height = 1 * unit;
  const int anchor_3_height = 1 * unit;
  const int pointer_down_y = 5 * unit;

  main_resource.Complete(String::Format(
      R"HTML(
    <body style="margin: 0px">
      <div style="height: %dpx;"></div>
      <a href="https://bar.com/1"
         style="height: %dpx; display: block;">
        one
      </a>
      <div style="height: %dpx;"></div>
      <a href="https://bar.com/2"
         style="height: %dpx; display: block;">
        two
      </a>
      <a href="https://bar.com/3"
         style="height: %dpx; display: block;">
        three
      </a>
    </body>
  )HTML",
      div_1_height, anchor_1_height, div_2_height, anchor_2_height,
      anchor_3_height));
  Compositor().BeginFrame();

  ProcessEvents(/*expected_anchors=*/3);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  auto& positions = mock_host->positions_;
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());

  HTMLCollection* anchors = GetDocument().links();
  EXPECT_EQ(3u, anchors->length());
  uint32_t anchor_1_id =
      AnchorElementId(To<HTMLAnchorElement>(*anchors->item(0)));
  uint32_t anchor_2_id =
      AnchorElementId(To<HTMLAnchorElement>(*anchors->item(1)));
  uint32_t anchor_3_id =
      AnchorElementId(To<HTMLAnchorElement>(*anchors->item(2)));

  auto get_distance_ratio = [&positions](uint32_t anchor_id) {
    auto it = positions.find(anchor_id);
    CHECK(it != positions.end());
    return it->second->distance_from_pointer_down_ratio.value();
  };

  auto get_position_ratio = [&positions](uint32_t anchor_id) {
    auto it = positions.find(anchor_id);
    CHECK(it != positions.end());
    return it->second->vertical_position_ratio;
  };

  // Simulate a pointer down and a scroll.
  //   XXXX   |  div_1
  // --------------------
  // viewport |  div_1
  //    ..    |  div_1
  //    ..    |  anchor_1
  //    ..    |  div_2
  //    ..    |  div_2          . pointerdown
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  anchor_2
  // ----------------------
  //   XXXX   |  anchor_3
  gfx::PointF coordinates(10.0f, pointer_down_y);
  gfx::PointF screen_coordinates(coordinates.x(), coordinates.y() + 2 * unit);
  WebInputEvent::Modifiers modifier = WebInputEvent::kLeftButtonDown;
  WebMouseEvent event(WebInputEvent::Type::kMouseDown, coordinates,
                      screen_coordinates, WebPointerProperties::Button::kLeft,
                      0, modifier, WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  VerticalScroll(-unit);
  ProcessPositionUpdates();

  EXPECT_EQ(2u, mock_host->entered_viewport_.size());
  EXPECT_EQ(2u, positions.size());
  EXPECT_FLOAT_EQ(-2.5f * unit / kViewportHeight,
                  get_distance_ratio(anchor_1_id));
  EXPECT_FLOAT_EQ(2.5f * unit / kViewportHeight,
                  get_position_ratio(anchor_1_id));
  EXPECT_FLOAT_EQ(2.5f * unit / kViewportHeight,
                  get_distance_ratio(anchor_2_id));
  EXPECT_FLOAT_EQ(7.5f * unit / kViewportHeight,
                  get_position_ratio((anchor_2_id)));
  // anchor_3 is not in the viewport, so a ratio isn't reported.
  EXPECT_TRUE(!base::Contains(positions, anchor_3_id));
  positions.clear();

  // Zoom (visual as opposed to logical), and scroll up by 2 units post-zoom.
  //         ...
  //   XXXX   |  div_1
  // --------------------
  // viewport |  div_1
  //    ..    |  div_1
  //    ..    |  anchor_1
  //    ..    |  anchor_1
  //    ..    |  div_2          . pointerdown
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2
  // ----------------------
  //   XXXX   |  div_2
  //   XXXX   |  div_2
  //   XXXX   |  div_2
  //   XXXX   |  div_2
  //   XXXX   |  anchor_2
  //   XXXX   |  anchor_2
  //         ...
  GetDocument().GetPage()->SetPageScaleFactor(2.0f);
  Compositor().BeginFrame();
  VerticalScroll(-2 * unit);
  ProcessPositionUpdates();

  EXPECT_EQ(2u, positions.size());
  EXPECT_FLOAT_EQ(-2.0f * unit / kViewportHeight,
                  get_distance_ratio(anchor_1_id));
  EXPECT_FLOAT_EQ(3.0f * unit / kViewportHeight,
                  get_position_ratio(anchor_1_id));
  // Note: anchor_2 is not in the visual viewport after the zoom, but is still
  // in the layout viewport (and will be considered as intersecting by
  // IntersectionObserver, so we still report a distance ratio).
  EXPECT_FLOAT_EQ(8.0f * unit / kViewportHeight,
                  get_distance_ratio(anchor_2_id));
  EXPECT_FLOAT_EQ(13.0f * unit / kViewportHeight,
                  get_position_ratio(anchor_2_id));
  EXPECT_TRUE(!base::Contains(positions, anchor_3_id));
}

// TODO(crbug.com/347719430): This test can be removed if
// LocalFrameView::FrameToViewport supports local root subframes with local
// main frames.
TEST_F(AnchorElementMetricsSenderTest,
       PositionUpdate_IgnorePointerDownInsideLocalRootSubframe) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNavigationPredictorNewViewportFeatures);

  ASSERT_EQ(0, kViewportHeight % 8);
  int unit = kViewportHeight / 8;
  int div_1_height = unit;
  int anchor_height = unit;
  int iframe_height = 3 * unit;
  int div_2_height = 8 * unit;

  // Navigate the main frame.
  String source("https://foo.com");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(R"HTML(
    <body style="margin: 0px">
      <div style="height: %dpx"></div>
      <a href="https://bar.com"
         style="height: %dpx; display: block;">Bar</a>
      <iframe height="%dpx;"></iframe>
      <div style="height: %dpx;"></div>
    </body>
  )HTML",
                                        div_1_height, anchor_height,
                                        iframe_height, div_2_height));
  EXPECT_EQ(1u, GetDocument().links()->length());

  // Make the iframe remote, and add a local child to it (the child is a local
  // root).
  WebRemoteFrameImpl* remote_child = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame().FirstChild(), remote_child);
  EXPECT_TRUE(MainFrame().FirstChild()->IsWebRemoteFrame());
  WebLocalFrameImpl* local_child =
      WebViewHelper().CreateLocalChild(*remote_child);
  local_child->FrameWidget()->Resize(gfx::Size(200, iframe_height));

  // Navigate the local root iframe.
  String iframe_source("https://foo.com/2");
  SimRequest iframe_resource(iframe_source, "text/html");
  frame_test_helpers::LoadFrameDontWait(local_child, KURL(iframe_source));
  iframe_resource.Complete(String::Format(R"HTML(
    <body>
      <div height="%dpx"></div>
    </body>
  )HTML",
                                          iframe_height * 2));

  Compositor().BeginFrame();
  ProcessEvents(/*expected_anchors=*/1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  auto& positions = mock_host->positions_;
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, positions.size());

  auto create_mouse_press = [](gfx::PointF coordinates) -> WebMouseEvent {
    return WebMouseEvent(WebInputEvent::Type::kMouseDown, coordinates,
                         coordinates, WebPointerProperties::Button::kLeft, 0,
                         WebInputEvent::kLeftButtonDown,
                         WebInputEvent::GetStaticTimeStampForTests());
  };

  // Dispatch 2 pointerdown events, the first to the main frame, and the second
  // to the local root subframe.
  WebMouseEvent press_1 = create_mouse_press(gfx::PointF(10.f, 6.f * unit));
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(press_1);
  WebMouseEvent press_2 = create_mouse_press(gfx::PointF(10.f, 4.f * unit));
  local_child->GetFrame()->GetEventHandler().HandleMousePressEvent(press_2);
  // Scroll to trigger computation and dispatch of position updates.
  VerticalScroll(-unit);
  ProcessPositionUpdates();

  EXPECT_EQ(1u, positions.size());
  // The distance should be calculated using press_1's coordinates and not
  // press_2 (even though press_2 was dispatched after) as press_2 was inside
  // a subframe whose local root is not the main frame.
  EXPECT_FLOAT_EQ(
      -5.5f * unit / kViewportHeight,
      positions.begin()->second->distance_from_pointer_down_ratio.value());
}

// TODO(crbug.com/347719430): This test can be removed if
// LocalFrameView::FrameToViewport supports local root subframes with local
// main frames.
TEST_F(AnchorElementMetricsSenderTest,
       PositionUpdate_NotComputedForAnchorInsideLocalRootSubframe) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNavigationPredictorNewViewportFeatures);

  // Navigate the main frame.
  String source("https://foo.com");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"html(
    <body>
      <iframe></iframe>
      <div style="height: 1000px"></div>
    </body>
  )html");

  // Make the subframe a remote frame.
  WebRemoteFrameImpl* remote_child = frame_test_helpers::CreateRemote();
  frame_test_helpers::SwapRemoteFrame(MainFrame().FirstChild(), remote_child);
  EXPECT_TRUE(MainFrame().FirstChild()->IsWebRemoteFrame());
  // Add a local subframe to the remote subframe, the local subframe is a local
  // root.
  WebLocalFrameImpl* local_child =
      WebViewHelper().CreateLocalChild(*remote_child);
  WebFrameWidget* widget = local_child->FrameWidget();
  ASSERT_TRUE(widget);
  gfx::Size local_child_size(200, 400);
  widget->Resize(local_child_size);
  // This is needed to make IntersectionObserver to observe the anchor element
  // inside the local subframe as intersecting the viewport.
  auto viewport_intersection_state =
      mojom::blink::ViewportIntersectionState::New();
  gfx::Rect viewport_intersection(local_child_size);
  viewport_intersection_state->viewport_intersection = viewport_intersection;
  viewport_intersection_state->main_frame_intersection = viewport_intersection;
  viewport_intersection_state->compositor_visible_rect = viewport_intersection;
  static_cast<WebFrameWidgetImpl*>(widget)->ApplyViewportIntersectionForTesting(
      std::move(viewport_intersection_state));

  // Navigate the local root.
  String iframe_source("https://foo.com/2");
  SimRequest iframe_resource(iframe_source, "text/html");
  frame_test_helpers::LoadFrameDontWait(local_child, KURL(iframe_source));
  iframe_resource.Complete(R"HTML(
    <body>
      <a href="https://bar.com"
         style="height: 75px; width: 60px; display: block;">Link</a>
    </body>
  )HTML");

  HTMLCollection* anchors = local_child->GetFrame()->GetDocument()->links();
  EXPECT_EQ(1u, anchors->length());

  Compositor().BeginFrame();
  local_child->GetFrame()->View()->UpdateAllLifecyclePhasesForTest();
  ProcessEvents(/*expected_anchors=*/1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  auto& positions = mock_host->positions_;
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());

  // Pointer down and scroll in the main frame.
  gfx::PointF coordinates(10.0f, 100.0f);
  WebInputEvent::Modifiers modifier = WebInputEvent::kLeftButtonDown;
  WebMouseEvent event(WebInputEvent::Type::kMouseDown, coordinates, coordinates,
                      WebPointerProperties::Button::kLeft, 0, modifier,
                      WebInputEvent::GetStaticTimeStampForTests());
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(event);
  VerticalScroll(-50.0f);
  ProcessPositionUpdates();

  // We should not get a position update for the anchor inside the local
  // subframe because its local root is not the main frame.
  EXPECT_EQ(0u, positions.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
}

TEST_F(AnchorElementMetricsSenderTest,
       PositionUpdate_BrowserTopControlsHeight) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNavigationPredictorNewViewportFeatures);

  ASSERT_EQ(0, kViewportHeight % 8);
  int unit = kViewportHeight / 8;
  const int top_controls_height = unit;

  // Set up the viewport as follows:
  //
  // controls |  XXXX
  // viewport |  div_1
  //    ..    |  div_1
  //    ..    |  div_1
  //    ..    |  anchor_1
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2
  // -------------------
  const int div_1_height = 3 * unit;
  const int anchor_height = unit;
  const int div_2_height = 8 * unit;

  WebView().ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight - top_controls_height),
      top_controls_height, /*bottom_controls_height=*/0,
      /*browser_controls_shrink_layout=*/true);
  BrowserControls& browser_controls = WebView().GetBrowserControls();
  EXPECT_TRUE(browser_controls.ShrinkViewport());
  browser_controls.SetShownRatio(1.f, 0.f);
  EXPECT_EQ(top_controls_height, browser_controls.ContentOffset());
  const VisualViewport& visual_viewport = GetPage().GetVisualViewport();
  EXPECT_EQ(kViewportHeight - top_controls_height,
            visual_viewport.Size().height());

  // Navigate the main frame.
  String source("https://foo.com");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(R"HTML(
    <body style="margin: 0px">
      <div style="height: %dpx"></div>
      <a href="https://bar.com"
         style="height: %dpx; display: block;">Bar</a>
      <div style="height: %dpx;"></div>
    </body>
  )HTML",
                                        div_1_height, anchor_height,
                                        div_2_height));
  EXPECT_EQ(1u, GetDocument().links()->length());

  Compositor().BeginFrame();
  ProcessEvents(/*expected_anchors=*/1);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());

  // Pointer down and scroll down by 3 units. The browser controls should be
  // hidden.
  //
  // viewport |  div_1
  //    ..    |  anchor_1
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2
  //    ..    |  div_2        . pointerdown
  //    ..    |  div_2
  //    ..    |  div_2
  // -------------------
  gfx::PointF coordinates(10.0f, 5.f * unit);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      WebMouseEvent(WebInputEvent::Type::kMouseDown, coordinates, coordinates,
                    WebPointerProperties::Button::kLeft, 0,
                    WebInputEvent::kLeftButtonDown,
                    WebInputEvent::GetStaticTimeStampForTests()));
  VerticalScroll(-3.f * unit);
  EXPECT_FLOAT_EQ(0.f, browser_controls.TopShownRatio());
  // Simulates the viewport size being updated after the top controls are hidden
  // (this happens through WidgetBase::UpdateVisualProperties in practice).
  WebView().ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight), top_controls_height,
      /*bottom_controls_height=*/0, /*browser_controls_shrink_layout=*/true);
  EXPECT_EQ(0, browser_controls.ContentOffset());
  EXPECT_EQ(kViewportHeight, visual_viewport.Size().height());
  ProcessPositionUpdates();

  const auto& positions = mock_host->positions_;
  EXPECT_FLOAT_EQ(
      -4.5f * unit / kViewportHeight,
      positions.begin()->second->distance_from_pointer_down_ratio.value());
  EXPECT_FLOAT_EQ(1.5f * unit / kViewportHeight,
                  positions.begin()->second->vertical_position_ratio);

  // Pointer down and scroll up by 2 units. The browser controls should be
  // back.
  //
  // controls | XXXX
  // viewport |  div_1
  //    ..    |  div_1
  //    ..    |  anchor_1
  //    ..    |  div_2
  //    ..    |  div_2        . pointerdown
  //    ..    |  div_2
  //    ..    |  div_2
  // -------------------
  coordinates = gfx::PointF(10.0f, 6.f * unit);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      WebMouseEvent(WebInputEvent::Type::kMouseDown, coordinates, coordinates,
                    WebPointerProperties::Button::kLeft, 0,
                    WebInputEvent::kLeftButtonDown,
                    WebInputEvent::GetStaticTimeStampForTests()));
  VerticalScroll(2.f * unit);
  EXPECT_FLOAT_EQ(1.f, browser_controls.TopShownRatio());
  WebView().ResizeWithBrowserControls(
      gfx::Size(kViewportWidth, kViewportHeight - top_controls_height),
      top_controls_height, /*bottom_controls_height=*/0,
      /*browser_controls_shrink_layout=*/true);
  EXPECT_EQ(top_controls_height, browser_controls.ContentOffset());
  EXPECT_EQ(kViewportHeight - top_controls_height,
            visual_viewport.Size().height());
  ProcessPositionUpdates();

  EXPECT_FLOAT_EQ(
      -2.5f * unit / kViewportHeight,
      positions.begin()->second->distance_from_pointer_down_ratio.value());
  EXPECT_FLOAT_EQ(3.5f * unit / kViewportHeight,
                  positions.begin()->second->vertical_position_ratio);
}

// Regression test for crbug.com/352973572.
TEST_F(AnchorElementMetricsSenderTest, SubframeWithObservedAnchorsDetached) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kNavigationPredictorNewViewportFeatures);

  // Navigate the main frame.
  String source("https://foo.com");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  const int scroll_height_px = 100;
  main_resource.Complete(String::Format(R"html(
    <body>
      <div style="height: %dpx;"></div>
      <iframe width="400px" height="400px"></iframe>
      <a href="https://foo.com/one">one</a>
      <div style="height: 1000px;"></div>
    </body>
  )html",
                                        scroll_height_px));

  String subframe_source("https://foo.com/iframe");
  SimRequest subframe_resource(subframe_source, "text/html");
  frame_test_helpers::LoadFrameDontWait(
      MainFrame().FirstChild()->ToWebLocalFrame(), KURL(subframe_source));
  subframe_resource.Complete(R"html(
    <body>
      <a href="https://foo.com/two">one</a>
    </body>
  )html");

  Compositor().BeginFrame();
  ProcessEvents(/*expected_anchors=*/2);

  WebLocalFrameImpl* subframe =
      To<WebLocalFrameImpl>(MainFrame().FirstChild()->ToWebLocalFrame());
  Persistent<Document> subframe_doc = subframe->GetFrame()->GetDocument();
  uint32_t subframe_anchor_id =
      AnchorElementId(To<HTMLAnchorElement>(*subframe_doc->links()->item(0)));

  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(2u, mock_host->entered_viewport_.size());
  EXPECT_EQ(0u, mock_host->left_viewport_.size());

  subframe->Detach();
  VerticalScroll(-scroll_height_px);

  ProcessEvents(/*expected_anchors=*/1);
  ProcessPositionUpdates();

  EXPECT_EQ(1u, mock_host->positions_.size());
  EXPECT_EQ(1u, mock_host->removed_anchor_ids_.size());
  EXPECT_TRUE(
      base::Contains(mock_host->removed_anchor_ids_, subframe_anchor_id));
}

TEST_F(AnchorElementMetricsSenderTest,
       AnchorsNotReportedAsRemovedWhenMainFrameIsDetached) {
  // Navigate the main frame.
  String source("https://foo.com");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(R"html(
    <body>
      <iframe width="400px" height="400px"></iframe>
      <a href="https://foo.com/one">one</a>
    </body>
  )html"));

  String subframe_source("https://foo.com/iframe");
  SimRequest subframe_resource(subframe_source, "text/html");
  frame_test_helpers::LoadFrameDontWait(
      MainFrame().FirstChild()->ToWebLocalFrame(), KURL(subframe_source));
  subframe_resource.Complete(R"html(
    <body>
      <a href="https://foo.com/two">two</a>
    </body>
  )html");

  Compositor().BeginFrame();
  ProcessEvents(/*expected_anchors=*/2);
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];

  Document* document = &GetDocument();
  LocalFrameView* view = GetDocument().View();
  AnchorElementMetricsSender* sender =
      AnchorElementMetricsSender::From(GetDocument());
  // Note: This relies on the microtask running after the subframe detaches (in
  // FrameLoader::DetachDocumentLoader), but before the main frame is detached.
  base::OnceClosure microtask = base::BindLambdaForTesting([view, sender]() {
    view->UpdateAllLifecyclePhasesForTest();
    sender->FireUpdateTimerForTesting();
  });
  static_cast<frame_test_helpers::TestWebFrameClient*>(
      MainFrame().FirstChild()->ToWebLocalFrame()->Client())
      ->SetFrameDetachedCallback(
          base::BindLambdaForTesting([&document, &microtask]() {
            document->GetAgent().event_loop()->EnqueueMicrotask(
                std::move(microtask));
          }));

  source = "https://foo.com/two";
  SimRequest main_resource_2(source, "text/html");
  LoadURL(source);
  main_resource_2.Complete(String::Format(R"html(
    <body>
      <div>second page</div>
    </body>
  )html"));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, mock_host->removed_anchor_ids_.size());
}

}  // namespace blink
