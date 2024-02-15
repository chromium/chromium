// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/contamination_delay_navigation_throttle.h"

#include "base/functional/bind.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"

namespace content {
namespace {

void SetTestLoadTiming(network::mojom::URLResponseHead& response,
                       base::Time start,
                       base::TimeTicks start_ticks) {
  response.request_time = start;
  response.response_time = start + base::Milliseconds(1050);
  response.load_timing.request_start = start_ticks;
  response.load_timing.connect_timing.domain_lookup_start = start_ticks;
  response.load_timing.connect_timing.domain_lookup_end =
      start_ticks + base::Milliseconds(100);
  response.load_timing.connect_timing.connect_start =
      start_ticks + base::Milliseconds(100);
  response.load_timing.connect_timing.ssl_start =
      start_ticks + base::Milliseconds(300);
  response.load_timing.connect_timing.ssl_end =
      start_ticks + base::Milliseconds(500);
  response.load_timing.connect_timing.connect_end =
      start_ticks + base::Milliseconds(500);
  response.load_timing.send_start = start_ticks + base::Milliseconds(500);
  response.load_timing.send_end = start_ticks + base::Milliseconds(550);
  response.load_timing.receive_headers_start =
      start_ticks + base::Milliseconds(1000);
  response.load_timing.receive_non_informational_headers_start =
      start_ticks + base::Milliseconds(1000);
  response.load_timing.receive_headers_end =
      start_ticks + base::Milliseconds(1050);
}

class ContaminationDelayNavigationThrottleTest
    : public RenderViewHostTestHarness {
 protected:
  ContaminationDelayNavigationThrottleTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(ContaminationDelayNavigationThrottleTest, DefersSinceSendStart) {
  GURL next_url("https://destination.test/");
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(next_url, main_rfh());
  navigation->SetAutoAdvance(false);

  // Fill in some realistic looking timestamps, and mark it as contaminated.
  base::Time start = base::Time::Now();
  base::TimeTicks start_ticks = base::TimeTicks::Now();
  static_cast<NavigationSimulatorImpl*>(navigation.get())
      ->set_response_postprocess_hook(base::BindRepeating(
          [](base::Time start, base::TimeTicks start_ticks,
             network::mojom::URLResponseHead& response) {
            SetTestLoadTiming(response, start, start_ticks);
            response.is_prefetch_with_cross_site_contamination = true;
          },
          start, start_ticks));

  task_environment()->FastForwardBy(base::Milliseconds(1050));

  // When we're ready to commit, we should be deferred for 550 ms
  // (receive_headers_end - send_start) but no longer.
  navigation->ReadyToCommit();
  EXPECT_TRUE(navigation->IsDeferred());
  task_environment()->FastForwardBy(base::Milliseconds(549));
  EXPECT_TRUE(navigation->IsDeferred());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(navigation->IsDeferred());
  navigation->Commit();
}

TEST_F(ContaminationDelayNavigationThrottleTest, IgnoresUncontaminated) {
  GURL next_url("https://destination.test/");
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(next_url, main_rfh());
  navigation->SetAutoAdvance(false);

  // Fill in some realistic looking timestamps, and mark it as uncontaminated.
  base::Time start = base::Time::Now();
  base::TimeTicks start_ticks = base::TimeTicks::Now();
  static_cast<NavigationSimulatorImpl*>(navigation.get())
      ->set_response_postprocess_hook(base::BindRepeating(
          [](base::Time start, base::TimeTicks start_ticks,
             network::mojom::URLResponseHead& response) {
            SetTestLoadTiming(response, start, start_ticks);
            response.is_prefetch_with_cross_site_contamination = false;
          },
          start, start_ticks));

  task_environment()->FastForwardBy(base::Milliseconds(1050));

  // There should be no delay.
  navigation->ReadyToCommit();
  EXPECT_FALSE(navigation->IsDeferred());
  navigation->Commit();
}

}  // namespace
}  // namespace content
