// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/recently_destroyed_hosts.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolation_context.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/storage_partition_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RecentlyDestroyedHostsTest : public testing::Test {
 protected:
  RecentlyDestroyedHostsTest()
      : task_environment_(BrowserTaskEnvironment::TimeSource::MOCK_TIME),
        instance_(RecentlyDestroyedHosts::GetInstance(&browser_context_)) {}

  void AddReuseInterval(base::TimeDelta interval) {
    instance_->AddReuseInterval(interval);
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  std::vector<base::TimeDelta> GetReuseIntervals() {
    std::vector<base::TimeDelta> intervals;
    for (auto& reuse_interval : instance_->reuse_intervals_) {
      intervals.push_back(reuse_interval.interval);
    }
    return intervals;
  }

  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  raw_ptr<RecentlyDestroyedHosts> instance_;
};

TEST_F(RecentlyDestroyedHostsTest,
       RecordMetricIfReusableHostRecentlyDestroyed) {
  const IsolationContext isolation_context(
      BrowsingInstanceId(1), &browser_context_,
      /*is_guest=*/false,
      /*is_fenced=*/false,
      OriginAgentClusterIsolationState::CreateForDefaultIsolation(
          &browser_context_));
  const ProcessLock process_lock = ProcessLock::Create(
      isolation_context,
      UrlInfo::CreateForTesting(GURL("https://www.google.com"),
                                CreateStoragePartitionConfigForTesting()));

  constexpr char kHistogramName[] =
      "SiteIsolation.ReusePendingOrCommittedSite."
      "TimeSinceReusableProcessDestroyed";
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();

  // No entries matching |process_lock| are in the list of recently destroyed
  // hosts, so histogram value |kRecentlyDestroyedNotFoundSentinel| should be
  // emitted.
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context_);
  constexpr base::TimeDelta kRecentlyDestroyedNotFoundSentinel =
      base::Seconds(20);
  histogram_tester->ExpectUniqueTimeSample(
      kHistogramName, kRecentlyDestroyedNotFoundSentinel, 1);

  // Add a RenderProcessHost for |process_lock| to RecentlyDestroyedHosts.
  MockRenderProcessHost host(&browser_context_, /*is_for_guests_only=*/false);
  host.SetProcessLock(isolation_context, process_lock);
  RecentlyDestroyedHosts::Add(
      &host, /*time_spent_running_unload_handlers=*/base::TimeDelta(),
      &browser_context_);

  // Histogram value 0 seconds should be emitted, because no time has passed
  // (in this test's mocked time) since |host| was added.
  histogram_tester = std::make_unique<base::HistogramTester>();
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context_);
  histogram_tester->ExpectUniqueTimeSample(kHistogramName, base::TimeDelta(),
                                           1);

  // Re-add |host| to RecentlyDestroyedHosts right before its storage timeout
  // expires.
  task_environment_.FastForwardBy(
      RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout -
      base::Seconds(1));
  RecentlyDestroyedHosts::Add(
      &host, /*time_spent_running_unload_handlers=*/base::TimeDelta(),
      &browser_context_);
  constexpr base::TimeDelta kReuseInterval = base::Seconds(5);
  task_environment_.FastForwardBy(kReuseInterval);
  histogram_tester = std::make_unique<base::HistogramTester>();
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context_);
  // |host| should still be in RecentlyDestroyedHosts because its storage time
  // was refreshed by the second call to Add(). The histogram should emit the
  // time since the last call to Add() matching |process_lock|.
  histogram_tester->ExpectUniqueTimeSample(kHistogramName, kReuseInterval, 1);

  // After the storage timeout passes, |host| should no longer be present.
  histogram_tester = std::make_unique<base::HistogramTester>();
  task_environment_.FastForwardBy(
      RecentlyDestroyedHosts::kRecentlyDestroyedStorageTimeout);
  RecentlyDestroyedHosts::RecordMetricIfReusableHostRecentlyDestroyed(
      base::TimeTicks::Now(), process_lock, &browser_context_);
  histogram_tester->ExpectUniqueTimeSample(
      kHistogramName, kRecentlyDestroyedNotFoundSentinel, 1);
}

TEST_F(RecentlyDestroyedHostsTest, AddReuseInterval) {
  const base::TimeDelta t1 = base::Seconds(4);
  const base::TimeDelta t2 = base::Seconds(5);
  const base::TimeDelta t3 = base::Seconds(6.7);
  const base::TimeDelta t4 = base::Seconds(8.2);
  const base::TimeDelta t5 = base::Seconds(10);
  const base::TimeDelta t6 = base::Seconds(11);

  AddReuseInterval(t2);
  EXPECT_THAT(GetReuseIntervals(), testing::ElementsAre(t2));

  // The list of reuse intervals should be sorted from smallest to largest.
  AddReuseInterval(t1);
  AddReuseInterval(t1);
  AddReuseInterval(t3);
  AddReuseInterval(t6);
  EXPECT_THAT(GetReuseIntervals(), testing::ElementsAre(t1, t1, t2, t3, t6));

  // When the list of reuse intervals is at its maximum size and a new entry is
  // added, it should replace the oldest entry.
  AddReuseInterval(t5);
  EXPECT_THAT(GetReuseIntervals(), testing::ElementsAre(t1, t1, t3, t5, t6));
  AddReuseInterval(t4);
  EXPECT_THAT(GetReuseIntervals(), testing::ElementsAre(t1, t3, t4, t5, t6));
}

}  // namespace content
