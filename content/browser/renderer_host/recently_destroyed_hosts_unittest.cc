// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/recently_destroyed_hosts.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/coop_coep_cross_origin_isolated_info.h"
#include "content/browser/isolation_context.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(RecentlyDestroyedHostsTest, RecordMetricIfReusableHostRecentlyDestroyed) {
  BrowserTaskEnvironment task_environment(
      BrowserTaskEnvironment::TimeSource::MOCK_TIME);
  TestBrowserContext browser_context;
  const IsolationContext isolation_context(BrowsingInstanceId(1),
                                           &browser_context);
  const ProcessLock process_lock = ProcessLock::Create(
      isolation_context,
      UrlInfo::CreateForTesting(GURL("https://www.google.com")),
      CoopCoepCrossOriginIsolatedInfo::CreateNonIsolated());

  constexpr char kHistogramName[] =
      "SiteIsolation.ReusePendingOrCommittedSite."
      "TimeSinceReusableProcessDestroyed";
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();

  // No entries matching |process_lock| are in the list of recently destroyed
  // hosts, so histogram value |kRecentlyDestroyedNotFoundSentinel| should be
  // emitted.
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context);
  constexpr base::TimeDelta kRecentlyDestroyedNotFoundSentinel =
      base::TimeDelta::FromSeconds(20);
  histogram_tester->ExpectUniqueTimeSample(
      kHistogramName, kRecentlyDestroyedNotFoundSentinel, 1);

  // Add a RenderProcessHost for |process_lock| to RecentlyDestroyedHosts.
  MockRenderProcessHost host(&browser_context, /*is_for_guests_only=*/false);
  host.SetProcessLock(isolation_context, process_lock);
  RecentlyDestroyedHosts::Add(
      &host, /*time_spent_in_delayed_shutdown=*/base::TimeDelta(),
      &browser_context);

  // Histogram value 0 seconds should be emitted, because no time has passed
  // (in this test's mocked time) since |host| was added.
  histogram_tester = std::make_unique<base::HistogramTester>();
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context);
  histogram_tester->ExpectUniqueTimeSample(kHistogramName, base::TimeDelta(),
                                           1);

  // Re-add |host| to RecentlyDestroyedHosts right before its storage timeout
  // expires.
  task_environment.FastForwardBy(
      RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout -
      base::TimeDelta::FromSeconds(1));
  RecentlyDestroyedHosts::Add(
      &host, /*time_spent_in_delayed_shutdown=*/base::TimeDelta(),
      &browser_context);
  constexpr base::TimeDelta kReuseInterval = base::TimeDelta::FromSeconds(5);
  task_environment.FastForwardBy(kReuseInterval);
  histogram_tester = std::make_unique<base::HistogramTester>();
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context);
  // |host| should still be in RecentlyDestroyedHosts because its storage time
  // was refreshed by the second call to Add(). The histogram should emit the
  // time since the last call to Add() matching |process_lock|.
  histogram_tester->ExpectUniqueTimeSample(kHistogramName, kReuseInterval, 1);

  // After the storage timeout passes, |host| should no longer be present.
  histogram_tester = std::make_unique<base::HistogramTester>();
  task_environment.FastForwardBy(
      RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout);
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context);
  histogram_tester->ExpectUniqueTimeSample(
      kHistogramName, kRecentlyDestroyedNotFoundSentinel, 1);
}

}  // namespace content
