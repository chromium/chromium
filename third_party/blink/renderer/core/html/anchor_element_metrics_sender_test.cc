// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
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

  void ReportNewAnchorElements(
      WTF::Vector<mojom::blink::AnchorElementMetricsPtr> elements) override {
    for (auto& element : elements) {
      // Ignore duplicates.
      if (anchor_ids_.find(element->anchor_id) != anchor_ids_.end()) {
        continue;
      }
      anchor_ids_.insert(element->anchor_id);
      elements_.emplace_back(std::move(element));
    }
  }

 public:
  std::vector<mojom::blink::AnchorElementClickPtr> clicks_;
  std::vector<mojom::blink::AnchorElementEnteredViewportPtr> entered_viewport_;
  std::vector<mojom::blink::AnchorElementMetricsPtr> elements_;
  std::set<int32_t> anchor_ids_;

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

  base::test::ScopedFeatureList feature_list_;
  std::vector<std::unique_ptr<MockAnchorElementMetricsHost>> hosts_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

// Test that anchors on non-HTTPS pages are not reported.
TEST_F(AnchorElementMetricsSenderTest, AddAnchorElementHTTP) {
  String source("http://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href=''>example</a><a id='anchor2' href=''>example</a>");

  ProcessEvents(0);
  EXPECT_EQ(0u, hosts_.size());
}

TEST_F(AnchorElementMetricsSenderTest, AddAnchorElement) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(
      "<a id='anchor1' href=''>example</a><a id='anchor2' href=''>example</a>");

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

TEST_F(AnchorElementMetricsSenderTest, AnchorElementEnteredViewportLater) {
  String source("https://example.com/p1");

  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(String::Format(
      R"HTML(
        <body style='margin: 0px'>
        <div style='height: %dpx;'></div>
        <a href="" style='width: 300px; height: %dpx;'>foo</a>
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
  // Wait until the script has had time to run.
  platform_->RunForPeriodSeconds(5.);
  next_page.Complete("empty");
  ProcessEvents(1);
  // The second page load has no anchor elements and therefore no host is bound.
  EXPECT_EQ(1u, hosts_.size());
  const auto& mock_host = hosts_[0];
  EXPECT_EQ(1u, mock_host->clicks_.size());
  EXPECT_EQ(1u, mock_host->entered_viewport_.size());
  EXPECT_EQ(1u, mock_host->elements_.size());
  EXPECT_EQ(mock_host->clicks_[0]->anchor_id,
            mock_host->elements_[0]->anchor_id);
}

}  // namespace blink
