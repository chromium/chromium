// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/document_load_timing.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class DocumentLoadTimingTest : public testing::Test {};

TEST_F(DocumentLoadTimingTest, ensureValidNavigationStartAfterEmbedder) {
  auto dummy_page = std::make_unique<DummyPageHolder>();
  DocumentLoadTiming timing(*(dummy_page->GetDocument().Loader()));

  double delta = -1000;
  double embedder_navigation_start =
      base::TimeTicks::Now().since_origin().InSecondsF() + delta;
  timing.SetNavigationStart(base::TimeTicks() + base::TimeDelta::FromSecondsD(
                                                    embedder_navigation_start));

  double real_wall_time = base::Time::Now().ToDoubleT();
  base::TimeDelta adjusted_wall_time =
      timing.MonotonicTimeToPseudoWallTime(timing.NavigationStart());

  EXPECT_NEAR(adjusted_wall_time.InSecondsF(), real_wall_time + delta, .001);
}

TEST_F(DocumentLoadTimingTest, correctTimingDeltas) {
  auto dummy_page = std::make_unique<DummyPageHolder>();
  DocumentLoadTiming timing(*(dummy_page->GetDocument().Loader()));

  double navigation_start_delta = -456;
  double current_monotonic_time =
      base::TimeTicks::Now().since_origin().InSecondsF();
  double embedder_navigation_start =
      current_monotonic_time + navigation_start_delta;

  timing.SetNavigationStart(base::TimeTicks() + base::TimeDelta::FromSecondsD(
                                                    embedder_navigation_start));

  // Super quick load! Expect the wall time reported by this event to be
  // dominated by the navigationStartDelta, but similar to currentTime().
  timing.MarkLoadEventEnd();
  double real_wall_load_event_end = base::Time::Now().ToDoubleT();
  base::TimeDelta adjusted_load_event_end =
      timing.MonotonicTimeToPseudoWallTime(timing.LoadEventEnd());

  EXPECT_NEAR(adjusted_load_event_end.InSecondsF(), real_wall_load_event_end,
              .001);

  base::TimeDelta adjusted_navigation_start =
      timing.MonotonicTimeToPseudoWallTime(timing.NavigationStart());
  EXPECT_NEAR(
      (adjusted_load_event_end - adjusted_navigation_start).InSecondsF(),
      -navigation_start_delta, .001);
}

TEST_F(DocumentLoadTimingTest, ensureRedirectEndExcludesNextFetch) {
  // Regression test for https://crbug.com/823254.

  auto dummy_page = std::make_unique<DummyPageHolder>();
  DocumentLoadTiming timing(*(dummy_page->GetDocument().Loader()));

  base::TimeTicks origin;
  auto t1 = base::TimeDelta::FromSeconds(5);
  auto t2 = base::TimeDelta::FromSeconds(10);

  // Start a navigation to |url_that_redirects|.
  timing.SetNavigationStart(origin);

  // Simulate a redirect taking |t1| seconds.
  timing.SetRedirectStart(origin);
  origin += t1;
  timing.SetRedirectEnd(origin);

  // Start fetching |url_that_loads|.
  timing.SetFetchStart(origin);

  // Track the redirection.
  KURL url_that_redirects("some_url");
  KURL url_that_loads("some_other_url");
  timing.AddRedirect(url_that_redirects, url_that_loads);

  // Simulate |t2| seconds elapsing between fetchStart and responseEnd.
  origin += t2;
  timing.SetResponseEnd(origin);

  // The bug was causing |redirectEnd| - |redirectStart| ~= |t1| + |t2| when it
  // should be just |t1|.
  double redirect_time_ms =
      (timing.RedirectEnd() - timing.RedirectStart()).InMillisecondsF();
  EXPECT_NEAR(redirect_time_ms, t1.InMillisecondsF(), 1.0);
}
}  // namespace blink
