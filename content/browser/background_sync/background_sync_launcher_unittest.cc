// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_launcher.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#if defined(OS_ANDROID)
#include "base/run_loop.h"
#endif

namespace content {

namespace {

const char kUrl_1[] = "https://example.com";
const char kUrl_2[] = "https://whereswaldo.com";

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;

  void GetStoragePartitionConfigForSite(BrowserContext* browser_context,
                                        const GURL& site,
                                        bool can_be_default,
                                        std::string* partition_domain,
                                        std::string* partition_name,
                                        bool* in_memory) override {
    DCHECK(browser_context);
    DCHECK(partition_domain);
    DCHECK(partition_name);

    auto partition_num = std::to_string(++partition_count_);
    *partition_domain = std::string("PartitionDomain") + partition_num;
    *partition_name = std::string("Partition") + partition_num;
    *in_memory = false;
  }

 private:
  int partition_count_ = 0;
};

}  // namespace

class BackgroundSyncLauncherTest : public testing::Test {
 public:
  BackgroundSyncLauncherTest()
      : task_environment_(BrowserTaskEnvironment::MainThreadType::UI) {}

  void SetUpBrowserContext(const std::vector<GURL>& urls,
                           blink::mojom::BackgroundSyncType sync_type,
                           const std::map<GURL, int>& wakeup_deltas = {}) {
    DCHECK(!urls.empty());

    for (const auto& url : urls) {
      auto* storage_partition = BrowserContext::GetStoragePartitionForSite(
          &test_browser_context_, url);

      auto iter = wakeup_deltas.find(url);
      if (iter == wakeup_deltas.end())
        continue;

      static_cast<StoragePartitionImpl*>(storage_partition)
          ->GetBackgroundSyncContext()
          ->set_wakeup_delta_for_testing(
              sync_type, base::TimeDelta::FromMilliseconds(iter->second));
    }
  }

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override { SetBrowserClientForTesting(original_client_); }

  base::TimeDelta GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType sync_type) {
    base::TimeDelta to_return;
    BackgroundSyncLauncher::GetSoonestWakeupDelta(
        sync_type, &test_browser_context_,
        base::BindLambdaForTesting(
            [&to_return](base::TimeDelta soonest_wakeup_delta) {
              to_return = soonest_wakeup_delta;
            }));
    task_environment_.RunUntilIdle();
    return to_return;
  }

#if defined(OS_ANDROID)
  void FireBackgroundSyncEventsForAllPartitions() {
    num_invocations_fire_background_sync_events_ = 0;

    auto done_closure = base::BindLambdaForTesting(
        [&]() { num_invocations_fire_background_sync_events_++; });

    BrowserContext::ForEachStoragePartition(
        &test_browser_context_,
        base::BindRepeating(
            [](base::OnceClosure done_closure,
               StoragePartition* storage_partition) {
              BackgroundSyncContext* sync_context =
                  storage_partition->GetBackgroundSyncContext();
              sync_context->FireBackgroundSyncEvents(
                  blink::mojom::BackgroundSyncType::ONE_SHOT,
                  std::move(done_closure));
            },
            std::move(done_closure)));

    task_environment_.RunUntilIdle();
  }

  int NumInvocationsOfFireBackgroundSyncEvents() {
    return num_invocations_fire_background_sync_events_;
  }
#endif

 protected:
  void DidFireBackgroundSyncEvents() {
    num_invocations_fire_background_sync_events_++;
  }

  BrowserTaskEnvironment task_environment_;
  TestBrowserClient browser_client_;
  ContentBrowserClient* original_client_;
  TestBrowserContext test_browser_context_;
  int num_invocations_fire_background_sync_events_ = 0;
};

// Tests that we pick the correct wake up delta for the one-shot Background
// Sync wake up task, across all storage partitions.
TEST_F(BackgroundSyncLauncherTest, CorrectSoonestWakeupDeltaIsPicked) {
  std::vector<GURL> urls = {GURL(kUrl_1), GURL(kUrl_2)};

  // Add two storage partitions. Verify that we set the soonest wake up delta
  // to base::TimeDelta::Max(). This will cause cancellation of the wakeup
  // task.
  SetUpBrowserContext(urls, blink::mojom::BackgroundSyncType::ONE_SHOT);
  EXPECT_TRUE(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::ONE_SHOT)
                  .is_max());

  // Add two more storage partitions, this time with wakeup_deltas.
  // Verify that we pick the smaller of the two.
  int delta_ms = 0;
  std::map<GURL, int> wakeup_deltas;
  for (const auto& url : urls)
    wakeup_deltas[url] = delta_ms += 1000;
  SetUpBrowserContext(urls, blink::mojom::BackgroundSyncType::ONE_SHOT,
                      wakeup_deltas);

  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::ONE_SHOT)
                .InMilliseconds(),
            1000);
}

// Tests that we pick the correct wake up delta for the correct Background Sync
// wake up task, across all storage partitions.
TEST_F(BackgroundSyncLauncherTest, SoonestWakeupDeltaIsPickedForTheRightTask) {
  std::vector<GURL> urls = {GURL(kUrl_1), GURL(kUrl_2)};

  // Add two storage partitions with wakeup_deltas, both of the same sync type.
  // Verify that we pick the smaller of the two.
  int delta_ms = 0;
  std::map<GURL, int> wakeup_deltas;
  for (const auto& url : urls)
    wakeup_deltas[url] = delta_ms += 1000;
  SetUpBrowserContext(urls, blink::mojom::BackgroundSyncType::ONE_SHOT,
                      wakeup_deltas);

  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::ONE_SHOT)
                .InMilliseconds(),
            1000);
  EXPECT_TRUE(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC)
                  .is_max());

  // Add some more wakeup_deltas now for Periodic Background Sync.
  wakeup_deltas.clear();
  wakeup_deltas[GURL(kUrl_1)] = 500;
  SetUpBrowserContext(urls, blink::mojom::BackgroundSyncType::PERIODIC,
                      wakeup_deltas);
  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::ONE_SHOT)
                .InMilliseconds(),
            1000);
  EXPECT_EQ(GetSoonestWakeupDelta(blink::mojom::BackgroundSyncType::PERIODIC)
                .InMilliseconds(),
            500);
}

#if defined(OS_ANDROID)
TEST_F(BackgroundSyncLauncherTest, FireBackgroundSyncEvents) {
  std::vector<GURL> urls = {GURL(kUrl_1), GURL(kUrl_2)};
  SetUpBrowserContext(urls, blink::mojom::BackgroundSyncType::ONE_SHOT);

  ASSERT_NO_FATAL_FAILURE(FireBackgroundSyncEventsForAllPartitions());
  EXPECT_EQ(NumInvocationsOfFireBackgroundSyncEvents(), 2);
}
#endif

}  // namespace content
