// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_scheduler.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_scheduler.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_background_sync_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    *partition_domain = "PartitionDomain" + site.spec();
    *partition_name = "Partition" + site.spec();
    *in_memory = false;
  }
};

}  // namespace

class BackgroundSyncSchedulerTest : public testing::Test {
 public:
  BackgroundSyncSchedulerTest()
      : task_environment_(BrowserTaskEnvironment::MainThreadType::UI) {}

  void ScheduleDelayedProcessing(const GURL& url,
                                 blink::mojom::BackgroundSyncType sync_type,
                                 base::TimeDelta delay,
                                 base::OnceClosure delayed_task) {
    auto* scheduler = BackgroundSyncScheduler::GetFor(&test_browser_context_);
    DCHECK(scheduler);
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartitionForSite(&test_browser_context_,
                                                   url));
    DCHECK(storage_partition);

    scheduler->ScheduleDelayedProcessing(storage_partition, sync_type, delay,
                                         std::move(delayed_task));
  }

  void CancelDelayedProcessing(const GURL& url,
                               blink::mojom::BackgroundSyncType sync_type) {
    auto* scheduler = BackgroundSyncScheduler::GetFor(&test_browser_context_);
    DCHECK(scheduler);
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetStoragePartitionForSite(&test_browser_context_,
                                                   url));
    DCHECK(storage_partition);

    scheduler->CancelDelayedProcessing(storage_partition, sync_type);
  }

  MockBackgroundSyncController* GetController() {
    return static_cast<MockBackgroundSyncController*>(
        test_browser_context_.GetBackgroundSyncController());
  }

  base::TimeDelta GetBrowserWakeupDelay(
      blink::mojom::BackgroundSyncType sync_type) {
    return GetController()->GetBrowserWakeupDelay(sync_type);
  }

  void SetUp() override {
    original_client_ = SetBrowserClientForTesting(&browser_client_);
  }

  void TearDown() override { SetBrowserClientForTesting(original_client_); }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserClient browser_client_;
  ContentBrowserClient* original_client_;
  TestBrowserContext test_browser_context_;
};

TEST_F(BackgroundSyncSchedulerTest, ScheduleInvokesCallback) {
  base::RunLoop run_loop;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BackgroundSyncSchedulerTest, ZeroDelayScheduleDoesNotInvokeCallback) {
  bool was_called = false;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta(),
      base::BindOnce([](bool* was_called) { *was_called = true; },
                     &was_called));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_called);
}

TEST_F(BackgroundSyncSchedulerTest, CancelDoesNotInvokeCallback) {
  bool was_called = false;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMinutes(1),
      base::BindOnce([](bool* was_called) { *was_called = true; },
                     &was_called));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(was_called);

  CancelDelayedProcessing(GURL(kUrl_1),
                          blink::mojom::BackgroundSyncType::ONE_SHOT);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_called);
}

TEST_F(BackgroundSyncSchedulerTest, SchedulingTwiceOverwritesTimer) {
  bool was_called = false;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromSeconds(1),
      base::BindOnce([](bool* was_called) { *was_called = true; },
                     &was_called));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(was_called);

  base::RunLoop run_loop;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(was_called);
}

TEST_F(BackgroundSyncSchedulerTest, MultipleStoragePartitions) {
  base::RunLoop run_loop_1, run_loop_2;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromSeconds(1), run_loop_1.QuitClosure());

  ScheduleDelayedProcessing(
      GURL(kUrl_2), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), run_loop_2.QuitClosure());

  run_loop_1.Run();
  run_loop_2.Run();
}

TEST_F(BackgroundSyncSchedulerTest, ScheduleBothTypesOfSync) {
  base::RunLoop run_loop_1, run_loop_2;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), run_loop_1.QuitClosure());
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::PERIODIC,
      base::TimeDelta::FromMilliseconds(1), run_loop_2.QuitClosure());
  run_loop_1.Run();
  run_loop_2.Run();
}

#if defined(OS_ANDROID)
TEST_F(BackgroundSyncSchedulerTest, BrowserWakeupScheduled) {
  ScheduleDelayedProcessing(GURL(kUrl_1),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromSeconds(1), base::DoNothing());

  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromSeconds(1));

  ScheduleDelayedProcessing(
      GURL(kUrl_2), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), base::DoNothing());
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMilliseconds(1));
}

TEST_F(BackgroundSyncSchedulerTest,
       BrowserWakeupScheduleSecondAfterFirstFinishes) {
  base::RunLoop run_loop_1;
  ScheduleDelayedProcessing(
      GURL(kUrl_1), blink::mojom::BackgroundSyncType::ONE_SHOT,
      base::TimeDelta::FromMilliseconds(1), run_loop_1.QuitClosure());
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMilliseconds(1));
  run_loop_1.Run();

  ScheduleDelayedProcessing(GURL(kUrl_2),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromMinutes(1), base::DoNothing());
  EXPECT_GT(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMilliseconds(1));
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMinutes(1));
}

TEST_F(BackgroundSyncSchedulerTest, BrowserWakeupScheduleOneOfEachType) {
  ScheduleDelayedProcessing(GURL(kUrl_1),
                            blink::mojom::BackgroundSyncType::PERIODIC,
                            base::TimeDelta::FromSeconds(1), base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::PERIODIC),
            base::TimeDelta::FromSeconds(1));

  ScheduleDelayedProcessing(GURL(kUrl_2),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromMinutes(1), base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMinutes(1));
}

TEST_F(BackgroundSyncSchedulerTest, BrowserWakeupScheduleThenCancel) {
  ScheduleDelayedProcessing(GURL(kUrl_1),
                            blink::mojom::BackgroundSyncType::PERIODIC,
                            base::TimeDelta::FromMinutes(1), base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::PERIODIC),
            base::TimeDelta::FromMinutes(1));

  CancelDelayedProcessing(GURL(kUrl_1),
                          blink::mojom::BackgroundSyncType::PERIODIC);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::PERIODIC),
            base::TimeDelta::Max());
}

TEST_F(BackgroundSyncSchedulerTest, CancelingOneTypeDoesNotAffectAnother) {
  ScheduleDelayedProcessing(GURL(kUrl_1),
                            blink::mojom::BackgroundSyncType::PERIODIC,
                            base::TimeDelta::FromMinutes(1), base::DoNothing());
  ScheduleDelayedProcessing(GURL(kUrl_2),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromSeconds(1), base::DoNothing());

  CancelDelayedProcessing(GURL(kUrl_1),
                          blink::mojom::BackgroundSyncType::PERIODIC);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::PERIODIC),
            base::TimeDelta::Max());
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromSeconds(1));
}

TEST_F(BackgroundSyncSchedulerTest,
       CancelingProcessingForOneStorageParitionUpdatesBrowserWakeup) {
  ScheduleDelayedProcessing(GURL(kUrl_1),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromMinutes(1), base::DoNothing());
  ScheduleDelayedProcessing(GURL(kUrl_2),
                            blink::mojom::BackgroundSyncType::ONE_SHOT,
                            base::TimeDelta::FromSeconds(1), base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromSeconds(1));

  CancelDelayedProcessing(GURL(kUrl_2),
                          blink::mojom::BackgroundSyncType::ONE_SHOT);
  base::RunLoop().RunUntilIdle();

  EXPECT_LE(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromMinutes(1));
  EXPECT_GT(GetBrowserWakeupDelay(blink::mojom::BackgroundSyncType::ONE_SHOT),
            base::TimeDelta::FromSeconds(1));
}

#endif

}  // namespace content
