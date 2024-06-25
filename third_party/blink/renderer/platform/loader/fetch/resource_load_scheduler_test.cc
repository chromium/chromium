// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"

#include <memory>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/loading_behavior_observer.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace {

class MockClient final : public GarbageCollected<MockClient>,
                         public ResourceLoadSchedulerClient {
 public:
  // A delegate that can be used to determine the order clients were run in.
  class MockClientDelegate {
    DISALLOW_NEW();

   public:
    MockClientDelegate() = default;
    ~MockClientDelegate() = default;

    void NotifyRun(MockClient* client) { client_order_.push_back(client); }

    // The call order that hte clients ran in.
    const HeapVector<Member<MockClient>>& client_order() {
      return client_order_;
    }

    void Trace(Visitor* visitor) const { visitor->Trace(client_order_); }

   private:
    HeapVector<Member<MockClient>> client_order_;
  };

  ~MockClient() = default;

  void SetDelegate(MockClientDelegate* delegate) { delegate_ = delegate; }

  void Run() override {
    if (delegate_)
      delegate_->NotifyRun(this);
    EXPECT_FALSE(was_run_);
    was_run_ = true;
  }
  bool WasRun() { return was_run_; }

  void Trace(Visitor* visitor) const override {
    ResourceLoadSchedulerClient::Trace(visitor);
    visitor->Trace(console_logger_);
  }

 private:
  Member<DetachableConsoleLogger> console_logger_ =
      MakeGarbageCollected<DetachableConsoleLogger>();
  // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (GC'ed type), so
  // there is no benefit to using a raw_ptr, only cost.
  // TODO(crbug.com/348793154): Remove once clang plugin no longer enforces
  // those.
  RAW_PTR_EXCLUSION MockClientDelegate* delegate_;
  bool was_run_ = false;
};

class LoadingBehaviorObserverImpl final
    : public GarbageCollected<LoadingBehaviorObserverImpl>,
      public LoadingBehaviorObserver {
 public:
  void DidObserveLoadingBehavior(LoadingBehaviorFlag behavior) override {
    loading_behavior_flag_ |= behavior;
  }

  int32_t loading_behavior_flag() const { return loading_behavior_flag_; }

 private:
  int32_t loading_behavior_flag_ = 0;
};

class ResourceLoadSchedulerTest : public testing::Test {
 public:
  class MockConsoleLogger final : public GarbageCollected<MockConsoleLogger>,
                                  public ConsoleLogger {
   public:
    bool HasMessage() const { return has_message_; }

   private:
    void AddConsoleMessageImpl(
        mojom::ConsoleMessageSource,
        mojom::ConsoleMessageLevel,
        const String&,
        bool discard_duplicates,
        std::optional<mojom::ConsoleMessageCategory> category) override {
      has_message_ = true;
    }
    void AddConsoleMessageImpl(ConsoleMessage*,
                               bool discard_duplicates) override {
      has_message_ = true;
    }
    bool has_message_ = false;
  };

  using ThrottleOption = ResourceLoadScheduler::ThrottleOption;
  void SetUp() override {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    properties->SetShouldBlockLoadingSubResource(true);
    auto frame_scheduler = std::make_unique<scheduler::FakeFrameScheduler>();
    console_logger_ = MakeGarbageCollected<MockConsoleLogger>();
    loading_observer_behavior_ =
        MakeGarbageCollected<LoadingBehaviorObserverImpl>();
    scheduler_ = MakeGarbageCollected<ResourceLoadScheduler>(
        ResourceLoadScheduler::ThrottlingPolicy::kTight,
        ResourceLoadScheduler::ThrottleOptionOverride::kNone,
        properties->MakeDetachable(), frame_scheduler.get(),
        *MakeGarbageCollected<DetachableConsoleLogger>(console_logger_),
        loading_observer_behavior_.Get());
    Scheduler()->SetOutstandingLimitForTesting(1);
    feature_list_.InitAndDisableFeature(
        features::kDelayLowPriorityRequestsAccordingToNetworkState);
  }
  void TearDown() override { Scheduler()->Shutdown(); }

  MockConsoleLogger* GetConsoleLogger() { return console_logger_; }
  ResourceLoadScheduler* Scheduler() { return scheduler_; }

  bool Release(ResourceLoadScheduler::ClientId client) {
    return Scheduler()->Release(
        client, ResourceLoadScheduler::ReleaseOption::kReleaseOnly,
        ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }
  bool ReleaseAndSchedule(ResourceLoadScheduler::ClientId client) {
    return Scheduler()->Release(
        client, ResourceLoadScheduler::ReleaseOption::kReleaseAndSchedule,
        ResourceLoadScheduler::TrafficReportHints::InvalidInstance());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  Persistent<MockConsoleLogger> console_logger_;
  Persistent<LoadingBehaviorObserverImpl> loading_observer_behavior_;
  Persistent<ResourceLoadScheduler> scheduler_;
};

TEST_F(ResourceLoadSchedulerTest, StopStoppableRequest) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  // A request that disallows throttling should be queued.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_FALSE(client1->WasRun());

  // Another request that disallows throttling, but allows stopping should also
  // be queued.
  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  // Another request that disallows throttling and stopping also should be run
  // even it makes the outstanding number reaches to the limit.
  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kCanNotBeStoppedOrThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_TRUE(client3->WasRun());

  // Call Release() with different options just in case.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(ReleaseAndSchedule(id2));
  EXPECT_TRUE(ReleaseAndSchedule(id3));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(Release(id1));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(Release(ResourceLoadScheduler::kInvalidClientId));

  EXPECT_FALSE(Release(static_cast<ResourceLoadScheduler::ClientId>(774)));
}

TEST_F(ResourceLoadSchedulerTest, ThrottleThrottleableRequest) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  Scheduler()->SetOutstandingLimitForTesting(0);
  // A request that allows throttling should be queued.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_FALSE(client1->WasRun());

  // Another request that disallows throttling also should be run even it makes
  // the outstanding number reaches to the limit.
  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_TRUE(client2->WasRun());

  // Another request that disallows stopping should be run even it makes the
  // outstanding number reaches to the limit.
  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kCanNotBeStoppedOrThrottled,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_TRUE(client3->WasRun());

  // Call Release() with different options just in case.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(ReleaseAndSchedule(id2));
  EXPECT_TRUE(ReleaseAndSchedule(id3));

  // Should not succeed to call with the same ID twice.
  EXPECT_FALSE(Release(id1));

  // Should not succeed to call with the invalid ID or unused ID.
  EXPECT_FALSE(Release(ResourceLoadScheduler::kInvalidClientId));

  EXPECT_FALSE(Release(static_cast<ResourceLoadScheduler::ClientId>(774)));
}

TEST_F(ResourceLoadSchedulerTest, Throttled) {
  // The first request should be ran synchronously.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  // Another request should be throttled until the first request calls Release.
  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  // Two more requests.
  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  MockClient* client4 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);
  EXPECT_FALSE(client4->WasRun());

  // Call Release() to run the second request.
  EXPECT_TRUE(ReleaseAndSchedule(id1));
  EXPECT_TRUE(client2->WasRun());

  // Call Release() with kReleaseOnly should not run the third and the fourth
  // requests.
  EXPECT_TRUE(Release(id2));
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  // Should be able to call Release() for a client that hasn't run yet. This
  // should run another scheduling to run the fourth request.
  EXPECT_TRUE(ReleaseAndSchedule(id3));
  EXPECT_TRUE(client4->WasRun());
}

TEST_F(ResourceLoadSchedulerTest, Unthrottle) {
  // Push three requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  // Allows to pass all requests.
  Scheduler()->SetOutstandingLimitForTesting(3);
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, Stopped) {
  // Push three requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(client1->WasRun());

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(client3->WasRun());

  // Setting outstanding_limit_ to 0 in ThrottlingState::kStopped, prevents
  // further requests.
  Scheduler()->SetOutstandingLimitForTesting(0);
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Calling Release() still does not run the second request.
  EXPECT_TRUE(ReleaseAndSchedule(id1));
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
}

TEST_F(ResourceLoadSchedulerTest, PriorityIsConsidered) {
  // This tests the request limiting logic in the scheduler for
  // the tight-mode and regular-mode limits as well as the
  // special-casing for medium-priority requests.

  // Allow 1 overall request as well as 1 special-case medium request
  // while blocking anly low-priority requests (starts in tight mode)
  Scheduler()->SetOutstandingLimitForTesting(
      0 /* tight_limit */, 1 /* normal_limit */, 1 /* tight_medium_limit */);

  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 3 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  MockClient* client5 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id5 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client5, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id5);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id5);

  MockClient* client6 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id6 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client6, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kMedium, 0 /* intra_priority */,
                       &id6);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id6);

  // Expect that all 3 kLow requests are held, the one kHigh request
  // is sent by the normal limit and one of the kMedium requests
  // was sent using the medium-specific limit.
  EXPECT_FALSE(client1->WasRun()); /* kLowest */
  EXPECT_FALSE(client2->WasRun()); /* kLow intra=1 */
  EXPECT_FALSE(client3->WasRun()); /* kLow intra=3 */
  EXPECT_TRUE(client4->WasRun());  /* kHigh - Newly run */
  EXPECT_TRUE(client5->WasRun());  /* kMedium - Newly run */
  EXPECT_FALSE(client6->WasRun()); /* kMedium */

  // Calling Release() on kMedium schedules another one.
  EXPECT_TRUE(ReleaseAndSchedule(id5));
  EXPECT_FALSE(client1->WasRun()); /* kLowest */
  EXPECT_FALSE(client2->WasRun()); /* kLow intra=1 */
  EXPECT_FALSE(client3->WasRun()); /* kLow intra=3 */
  // 4 and 5 were already run and checked
  EXPECT_TRUE(client6->WasRun()); /* kMedium - Newly run */

  // Calling Release() on the last kMedium does not schedule non-medium.
  EXPECT_TRUE(ReleaseAndSchedule(id6));
  EXPECT_FALSE(client1->WasRun()); /* kLowest */
  EXPECT_FALSE(client2->WasRun()); /* kLow intra=1 */
  EXPECT_FALSE(client3->WasRun()); /* kLow intra=3 */
  // 4-6 have already run and been run and validated

  // Increasing the limit to 2 should allow another low-priority request
  // through, in order of priority (client 3 with the highest intra-priority).
  Scheduler()->SetOutstandingLimitForTesting(2);
  EXPECT_FALSE(client1->WasRun()); /* kLowest */
  EXPECT_FALSE(client2->WasRun()); /* kLow intra=1 */
  EXPECT_TRUE(client3->WasRun());  /* kLow intra=3 - Newly run */

  // Increasing the limit to 3 should allow another low-priority request
  // through, in order of priority (client 2).
  Scheduler()->SetOutstandingLimitForTesting(3);
  EXPECT_FALSE(client1->WasRun()); /* kLowest */
  EXPECT_TRUE(client2->WasRun());  /* kLow intra=1 - Newly run */
  // 3-6 have already run and been run and validated

  // Increasing the limit to 4 should allow the final (lowest-priority)
  // request through.
  Scheduler()->SetOutstandingLimitForTesting(4);
  EXPECT_TRUE(client1->WasRun());

  // Release the rest.
  EXPECT_TRUE(Release(id4));
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, AllowedRequestsRunInPriorityOrder) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  Scheduler()->SetOutstandingLimitForTesting(0);

  MockClient::MockClientDelegate delegate;
  // Push two requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  MockClient* client2 = MakeGarbageCollected<MockClient>();

  client1->SetDelegate(&delegate);
  client2->SetDelegate(&delegate);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(1);

  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(Release(id2));

  // Verify high priority request ran first.
  auto& order = delegate.client_order();
  EXPECT_EQ(order[0], client2);
  EXPECT_EQ(order[1], client1);
}

TEST_F(ResourceLoadSchedulerTest, StoppableRequestResumesWhenThrottled) {
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kStopped);
  // Push two requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();

  Scheduler()->SetOutstandingLimitForTesting(0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 1 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kStoppable,
                       ResourceLoadPriority::kLowest, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(1);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_TRUE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id1));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id3));
}

TEST_F(ResourceLoadSchedulerTest, SetPriority) {
  // Push three requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();

  // Allows one kHigh priority request by limits below.
  Scheduler()->SetOutstandingLimitForTesting(0, 1);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->SetPriority(id1, ResourceLoadPriority::kHigh, 0);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  Scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Loosen the policy to adopt the normal limit for all. Two requests
  // regardless of priority can be granted (including the in-flight high
  // priority request).
  Scheduler()->LoosenThrottlingPolicy();
  Scheduler()->SetOutstandingLimitForTesting(0, 2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // kHigh priority does not help the third request here.
  Scheduler()->SetPriority(id3, ResourceLoadPriority::kHigh, 0);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, LoosenThrottlingPolicy) {
  MockClient* client1 = MakeGarbageCollected<MockClient>();

  Scheduler()->SetOutstandingLimitForTesting(0, 0);

  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  MockClient* client4 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client4, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);

  Scheduler()->SetPriority(id2, ResourceLoadPriority::kLow, 0);
  Scheduler()->SetPriority(id3, ResourceLoadPriority::kLow, 0);
  Scheduler()->SetPriority(id4, ResourceLoadPriority::kMedium, 0);

  // As the policy is |kTight|, |kMedium| is throttled.
  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(0, 2);

  // The initial scheduling policy is |kTight|, setting the
  // outstanding limit for the normal mode doesn't take effect.
  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  // Now let's tighten the limit again.
  Scheduler()->SetOutstandingLimitForTesting(0, 0);

  // ...and change the scheduling policy to |kNormal|.
  Scheduler()->LoosenThrottlingPolicy();

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_FALSE(client4->WasRun());

  Scheduler()->SetOutstandingLimitForTesting(0, 2);

  EXPECT_FALSE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  // Release all.
  EXPECT_TRUE(Release(id4));
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest, ConsoleMessage) {
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  // Use a mock clock to control the time.
  Scheduler()->SetClockForTesting(test_task_runner->GetMockClock());

  Scheduler()->SetOutstandingLimitForTesting(0, 0);
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  // Push two requests into the queue.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_FALSE(client1->WasRun());

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLowest, 0 /* intra_priority */,
                       &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_FALSE(client2->WasRun());

  // Cancel the first request
  EXPECT_TRUE(Release(id1));

  // Advance current time a little and triggers an life cycle event, but it
  // still won't awake the warning logic.
  test_task_runner->FastForwardBy(base::Seconds(50));
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(GetConsoleLogger()->HasMessage());
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  // Modify current time to awake the console warning logic, and the second
  // client should be used for console logging.
  test_task_runner->FastForwardBy(base::Seconds(15));
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_TRUE(GetConsoleLogger()->HasMessage());
  EXPECT_TRUE(Release(id2));

  // Reset the reference to ensure scheduler won't keep a reference to the
  // destroyed clock.
  Scheduler()->SetClockForTesting(nullptr);
}

TEST_F(ResourceLoadSchedulerTest, ConsiderNetworkStateInTigtMode) {
  const base::FieldTrialParams network_params = {
      {features::kMaxNumOfThrottleableRequestsInTightMode.name, "2"},
      {features::kHttpRttThreshold.name, "3600ms"},
      {features::kCostReductionOfMultiplexedRequests.name, "0.5"}};

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kDelayLowPriorityRequestsAccordingToNetworkState,
        network_params}},
      {});

  Scheduler()->SetOutstandingLimitForTesting(2, 5);

  // Sets the RTT.
  Scheduler()->SetHttpRttForTesting(base::Milliseconds(1000));

  // Push 2 requests, 1 non-multiplexed request and the other is multiplexed.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id2);
  Scheduler()->SetConnectionInfo(id2, net::HttpConnectionInfo::kHTTP2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());

  // Continue to push another non-multiplexed request, because there is
  // already a multiplexed request, which is`id2`, the newly added one can
  // still be handled without being delayed.
  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 10 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);

  EXPECT_TRUE(client3->WasRun());

  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_F(ResourceLoadSchedulerTest,
       ConsiderNetworkStateInTigtModeWithPoorConnection) {
  const base::FieldTrialParams network_params = {
      {features::kMaxNumOfThrottleableRequestsInTightMode.name, "2"},
      {features::kHttpRttThreshold.name, "3600ms"},
      {features::kCostReductionOfMultiplexedRequests.name, "0.5"}};

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kDelayLowPriorityRequestsAccordingToNetworkState,
        network_params}},
      {});

  Scheduler()->SetOutstandingLimitForTesting(2, 1024);

  // Sets the RTT as a slow connection.
  Scheduler()->SetHttpRttForTesting(base::Milliseconds(5000));

  // Push three requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kHigh, 0 /* intra_priority */,
                       &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);

  MockClient* client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id2);
  Scheduler()->SetConnectionInfo(id2, net::HttpConnectionInfo::kHTTP2);

  // This request will not run, because we are experiencing a slow connection.
  MockClient* client3 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(client3, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 5 /* intra_priority */,
                       &id3);
  Scheduler()->SetConnectionInfo(id3, net::HttpConnectionInfo::kHTTP2);

  EXPECT_TRUE(client1->WasRun());
  EXPECT_TRUE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());

  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

}  // namespace
}  // namespace blink
