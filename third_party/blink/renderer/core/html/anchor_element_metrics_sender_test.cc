// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

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

class AnchorElementMetricsSenderTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 600;

 protected:
  AnchorElementMetricsSenderTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    // Report all anchors to avoid non-deterministic behavior.
    std::map<std::string, std::string> params;
    params["random_anchor_sampling_period"] = "1";

    feature_list_.InitAndEnableFeatureWithParameters(
        features::kNavigationPredictor, params);

    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);

    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));

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
  base::test::ScopedFeatureList anchor_element_interaction;
  anchor_element_interaction.InitWithFeatures(
      {features::kAnchorElementInteraction,
       features::kSpeculationRulesPointerHoverHeuristics},
      {});

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

}  // namespace blink
