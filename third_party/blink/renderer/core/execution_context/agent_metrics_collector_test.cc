// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/agent_metrics_collector.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/default_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class AgentMetricsCollectorUnitTest : public SimTest {
 public:
  AgentMetricsCollectorUnitTest() = default;

  void SetUp() override {
    SimTest::SetUp();

    tick_clock_.SetNowTicks(base::TimeTicks::Now());

    // Tests turn this on but it would force all frames into a single agent.
    // Turn it off so we get an agent per-origin.
    WebView().GetPage()->GetSettings().SetAllowUniversalAccessFromFileURLs(
        false);

    WebView().GetPage()->GetAgentMetricsCollector()->SetTickClockForTesting(
        &tick_clock_);
    WebView().MainFrameWidget()->Resize(WebSize(800, 600));
  }

  void TearDown() override {
    if (!torn_down_) {
      // Avoid UAF when tick_clock_ is destroyed.
      auto* collector = WebView().GetPage()->GetAgentMetricsCollector();

      SimTest::TearDown();
      torn_down_ = true;

      collector->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
    }
  }

  bool torn_down_ = false;

  HistogramTester tester_;
  const char* kHistogramName = "PerformanceManager.AgentsPerRendererByTime";

  base::SimpleTestTickClock tick_clock_;
};

// Tests that we record samples across a navigation.
TEST_F(AgentMetricsCollectorUnitTest, AgentsPerRendererRecordOnDocumentChange) {
  SimRequest request_a("https://example.com/a.html", "text/html");
  SimRequest request_b("https://example.com/b.html", "text/html");

  // Load the first page. The histogram won't yet have any data because it
  // records samples by time and only when documents are created/destroyed.
  // Immediately after we load a.html, no time has passed yet.
  LoadURL("https://example.com/a.html");
  request_a.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));

  // Load the second page. Since 10 seconds have now elapsed, as the documents
  // are swapped we should see 10 samples recorded in the 1 agent bucket.
  LoadURL("https://example.com/b.html");
  request_b.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  tester_.ExpectUniqueSample(kHistogramName, 1, 10);
}

// Test that we correctly record the case where a second agent is added to the
// page.
TEST_F(AgentMetricsCollectorUnitTest, MultipleAgents) {
  SimRequest request_a("https://foo.com/a.html", "text/html");
  SimRequest request_b("https://bar.com/b.html", "text/html");

  // Load the first page. The histogram won't yet have any data because it
  // records samples by time and only when documents are created/destroyed.
  // Immediately after we load a.html, no time has passed yet.
  LoadURL("https://foo.com/a.html");
  request_a.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe src="about:blank"></iframe>
  )HTML");
  Compositor().BeginFrame();

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));

  // Navigate to a cross-origin page, this should create a second agent.
  frame_test_helpers::LoadFrameDontWait(
      MainFrame().FirstChild()->ToWebLocalFrame(),
      KURL("https://bar.com/b.html"));
  request_b.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  tester_.ExpectBucketCount(kHistogramName, 1, 10);

  tick_clock_.Advance(base::TimeDelta::FromSeconds(20));

  // Simulate closing the page. This should cause us to report the metrics.
  TearDown();

  // The final 20 seconds had 2 agents.
  tester_.ExpectBucketCount(kHistogramName, 1, 10);
  tester_.ExpectBucketCount(kHistogramName, 2, 20);
}

// Ensure that multiple Pages in the same Agent are reported as only one agent.
TEST_F(AgentMetricsCollectorUnitTest, WindowOpenSameAgents) {
  SimRequest request_a("https://example.com/a.html", "text/html");
  SimRequest request_b("https://example.com/b.html", "text/html");

  LoadURL("https://example.com/a.html");
  request_a.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      window.open('https://example.com/b.html');
    </script>
  )HTML");
  request_b.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(2u, Page::OrdinaryPages().size());

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));

  // Simulate closing the page. This should cause us to report the metrics.
  TearDown();

  // Both documents should end up in the same Agent, despite having separate
  // WebViews/Page.
  tester_.ExpectUniqueSample(kHistogramName, 1, 10);
}

// Ensure that multiple Pages in different Agents are reported as multiple
// agents.
TEST_F(AgentMetricsCollectorUnitTest, WindowOpenDifferentAgents) {
  SimRequest request_a("https://example.com/a.html", "text/html");
  SimRequest request_b("https://different.com/a.html", "text/html");

  LoadURL("https://example.com/a.html");
  request_a.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      window.open('https://different.com/a.html');
    </script>
  )HTML");
  request_b.Complete(R"HTML(
    <!DOCTYPE html>
  )HTML");
  Compositor().BeginFrame();

  ASSERT_EQ(2u, Page::OrdinaryPages().size());

  tick_clock_.Advance(base::TimeDelta::FromSeconds(10));

  // Simulate closing the page. This should cause us to report the metrics.
  TearDown();

  // Each document should have its own Agent.
  tester_.ExpectUniqueSample(kHistogramName, 2, 10);
}

}  // namespace blink
