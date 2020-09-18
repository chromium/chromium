// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"

#include <memory>
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/loading_behavior_observer.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
  MockClientDelegate* delegate_;
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

class ResourceLoadSchedulerTestBase : public testing::Test {
 public:
  class MockConsoleLogger final : public GarbageCollected<MockConsoleLogger>,
                                  public ConsoleLogger {
   public:
    bool HasMessage() const { return has_message_; }

   private:
    void AddConsoleMessageImpl(mojom::ConsoleMessageSource,
                               mojom::ConsoleMessageLevel,
                               const String&,
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
    scheduler_->SetOptimizationGuideHints(std::move(optimization_hints_));
    Scheduler()->SetOutstandingLimitForTesting(1);
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

  bool WasDelayCompetingLowPriorityRequestsObserved() {
    return loading_observer_behavior_->loading_behavior_flag() &
           kLoadingBehaviorCompetingLowPriorityRequestsDelayed;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  Persistent<MockConsoleLogger> console_logger_;
  Persistent<LoadingBehaviorObserverImpl> loading_observer_behavior_;
  Persistent<ResourceLoadScheduler> scheduler_;
  mojom::blink::DelayCompetingLowPriorityRequestsHintsPtr optimization_hints_;
};

class ResourceLoadSchedulerTest
    : public ResourceLoadSchedulerTestBase,
      public testing::WithParamInterface<
          std::tuple<base::test::ScopedFeatureList::FeatureAndParams, bool>> {
 public:
  void SetUp() override {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        features_with_params;
    std::vector<base::Feature> disabled_features;

    bool enabled = std::get<1>(GetParam());
    if (enabled) {
      features_with_params.push_back(std::get<0>(GetParam()));
    } else {
      disabled_features.push_back(std::get<0>(GetParam()).feature);
    }

    feature_list_.InitWithFeaturesAndParameters(features_with_params,
                                                disabled_features);
    ResourceLoadSchedulerTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ResourceLoadSchedulerTest,
    testing::Combine(
        // A list of FeatureAndParams structs representing a single feature and
        // an arbitrary number of parameters.
        testing::Values(base::test::ScopedFeatureList::FeatureAndParams(
            features::kDelayCompetingLowPriorityRequests,
            {{"delay_type", "always"}})),
        // A boolean indicating whether or not the feature is enabled.
        testing::Bool()));

TEST_P(ResourceLoadSchedulerTest, StopStoppableRequest) {
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

TEST_P(ResourceLoadSchedulerTest, ThrottleThrottleableRequest) {
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

TEST_P(ResourceLoadSchedulerTest, Throttled) {
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

TEST_P(ResourceLoadSchedulerTest, Unthrottle) {
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

TEST_P(ResourceLoadSchedulerTest, Stopped) {
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

TEST_P(ResourceLoadSchedulerTest, PriorityIsConsidered) {
  // Push three requests.
  MockClient* client1 = MakeGarbageCollected<MockClient>();

  // Allows one kHigh priority request by limits below.
  Scheduler()->SetOutstandingLimitForTesting(0, 1);

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

  EXPECT_FALSE(client1->WasRun());
  EXPECT_FALSE(client2->WasRun());
  EXPECT_FALSE(client3->WasRun());
  EXPECT_TRUE(client4->WasRun());

  if (base::FeatureList::IsEnabled(
          features::kDelayCompetingLowPriorityRequests)) {
    // Allows two requests (regardless of priority). No other clients are run,
    // because the kHigh request is considered running / in-flight, so the kLow
    // priority request should be delayed behind it as per this feature.
    Scheduler()->SetOutstandingLimitForTesting(2);

    EXPECT_FALSE(client1->WasRun());
    EXPECT_FALSE(client2->WasRun());
    EXPECT_FALSE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    // ResourceLoadPriority::kLow requests will not run until client4 is
    // released.
    EXPECT_TRUE(ReleaseAndSchedule(id4));

    EXPECT_FALSE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_TRUE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    Scheduler()->SetOutstandingLimitForTesting(3);

    EXPECT_TRUE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_TRUE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    EXPECT_TRUE(WasDelayCompetingLowPriorityRequestsObserved());
  } else {
    Scheduler()->SetOutstandingLimitForTesting(2);

    EXPECT_FALSE(client1->WasRun());
    EXPECT_FALSE(client2->WasRun());
    EXPECT_TRUE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    Scheduler()->SetOutstandingLimitForTesting(3);

    EXPECT_FALSE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_TRUE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    Scheduler()->SetOutstandingLimitForTesting(4);

    EXPECT_TRUE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_TRUE(client3->WasRun());
    EXPECT_TRUE(client4->WasRun());

    EXPECT_FALSE(WasDelayCompetingLowPriorityRequestsObserved());
  }

  // Release the rest.
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id1));
}

TEST_P(ResourceLoadSchedulerTest, AllowedRequestsRunInPriorityOrder) {
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

  if (base::FeatureList::IsEnabled(
          features::kDelayCompetingLowPriorityRequests)) {
    EXPECT_FALSE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());

    EXPECT_TRUE(ReleaseAndSchedule(id2));
    EXPECT_TRUE(client1->WasRun());

    // Finish releasing all.
    EXPECT_TRUE(Release(id1));

    EXPECT_TRUE(WasDelayCompetingLowPriorityRequestsObserved());
  } else {
    EXPECT_TRUE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());

    // Release all.
    EXPECT_TRUE(Release(id1));
    EXPECT_TRUE(Release(id2));

    EXPECT_FALSE(WasDelayCompetingLowPriorityRequestsObserved());
  }

  // Verify high priority request ran first.
  auto& order = delegate.client_order();
  EXPECT_EQ(order[0], client2);
  EXPECT_EQ(order[1], client1);
}

TEST_P(ResourceLoadSchedulerTest, StoppableRequestResumesWhenThrottled) {
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

TEST_P(ResourceLoadSchedulerTest, SetPriority) {
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

  if (base::FeatureList::IsEnabled(
          features::kDelayCompetingLowPriorityRequests)) {
    // Loosen the policy to adopt the normal limit for all. One request
    // regardless of priority can be granted.
    Scheduler()->LoosenThrottlingPolicy();

    // The kLow requests are still delayed behind the in-flight kHigh one, since
    // it hasn't been released yet.
    EXPECT_TRUE(client1->WasRun());
    EXPECT_FALSE(client2->WasRun());
    EXPECT_FALSE(client3->WasRun());

    // Releasing the in-flight high priority request makes room for the next two
    // low priority requests to be granted, since the limit is two, and low
    // priority.
    EXPECT_TRUE(ReleaseAndSchedule(id1));
    EXPECT_TRUE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_FALSE(client3->WasRun());

    // kHigh priority does not help the third request here.
    Scheduler()->SetPriority(id3, ResourceLoadPriority::kHigh, 0);

    EXPECT_TRUE(client1->WasRun());
    EXPECT_TRUE(client2->WasRun());
    EXPECT_FALSE(client3->WasRun());

    // Release remaining clients.
    EXPECT_TRUE(Release(id3));
    EXPECT_TRUE(Release(id2));

    EXPECT_FALSE(WasDelayCompetingLowPriorityRequestsObserved());
  } else {
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

    EXPECT_FALSE(WasDelayCompetingLowPriorityRequestsObserved());
  }
}

TEST_P(ResourceLoadSchedulerTest, LoosenThrottlingPolicy) {
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

TEST_P(ResourceLoadSchedulerTest, ConsoleMessage) {
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
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
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(50));
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_FALSE(GetConsoleLogger()->HasMessage());
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kThrottled);

  // Modify current time to awake the console warning logic, and the second
  // client should be used for console logging.
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(15));
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  EXPECT_TRUE(GetConsoleLogger()->HasMessage());
  EXPECT_TRUE(Release(id2));
}

mojom::blink::DelayCompetingLowPriorityRequestsHintsPtr
CreateOptimizationGuideHints(
    features::DelayCompetingLowPriorityRequestsDelayType delay_milestone,
    features::DelayCompetingLowPriorityRequestsThreshold priority_threshold) {
  auto optimization_hints =
      mojom::blink::DelayCompetingLowPriorityRequestsHints::New();

  switch (delay_milestone) {
    case features::DelayCompetingLowPriorityRequestsDelayType::kFirstPaint:
      optimization_hints->delay_type =
          mojom::blink::DelayCompetingLowPriorityRequestsDelayType::kFirstPaint;
      break;
    case features::DelayCompetingLowPriorityRequestsDelayType::
        kFirstContentfulPaint:
      optimization_hints->delay_type = mojom::blink::
          DelayCompetingLowPriorityRequestsDelayType::kFirstContentfulPaint;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (priority_threshold) {
    case features::DelayCompetingLowPriorityRequestsThreshold::kMedium:
      optimization_hints->priority_threshold = mojom::blink::
          DelayCompetingLowPriorityRequestsPriorityThreshold::kMedium;
      break;
    case features::DelayCompetingLowPriorityRequestsThreshold::kHigh:
      optimization_hints->priority_threshold = mojom::blink::
          DelayCompetingLowPriorityRequestsPriorityThreshold::kHigh;
      break;
  }

  return optimization_hints;
}

class ResourceLoadSchedulerTestDelayCompetingLowPriorityRequests
    : public ResourceLoadSchedulerTestBase,
      public testing::WithParamInterface<
          std::tuple<bool /* use_optimization_guide */,
                     features::DelayCompetingLowPriorityRequestsDelayType,
                     features::DelayCompetingLowPriorityRequestsThreshold>> {
 public:
  void SetUp() override {
    std::map<std::string, std::string> parameters;
    bool use_optimization_guide = std::get<0>(GetParam());
    until_ = std::get<1>(GetParam());
    priority_threshold_ = std::get<2>(GetParam());

    if (use_optimization_guide) {
      parameters[features::kDelayCompetingLowPriorityRequestsDelayParam.name] =
          "use_optimization_guide";
      optimization_hints_ =
          CreateOptimizationGuideHints(until_, priority_threshold_);
    } else {
      switch (until_) {
        case features::DelayCompetingLowPriorityRequestsDelayType::kFirstPaint:
          parameters[features::kDelayCompetingLowPriorityRequestsDelayParam
                         .name] = "first_paint";
          break;
        case features::DelayCompetingLowPriorityRequestsDelayType::
            kFirstContentfulPaint:
          parameters[features::kDelayCompetingLowPriorityRequestsDelayParam
                         .name] = "first_contentful_paint";
          break;
        default:
          NOTREACHED();
          break;
      }
      switch (priority_threshold_) {
        case features::DelayCompetingLowPriorityRequestsThreshold::kMedium:
          parameters[features::kDelayCompetingLowPriorityRequestsThresholdParam
                         .name] = "medium";
          break;
        case features::DelayCompetingLowPriorityRequestsThreshold::kHigh:
          parameters[features::kDelayCompetingLowPriorityRequestsThresholdParam
                         .name] = "high";
          break;
      }
    }

    feature_list_.InitWithFeaturesAndParameters(
        {{features::kDelayCompetingLowPriorityRequests, parameters}}, {});
    ASSERT_TRUE(base::FeatureList::IsEnabled(
        features::kDelayCompetingLowPriorityRequests));
    if (use_optimization_guide) {
      ASSERT_EQ(features::kDelayCompetingLowPriorityRequestsDelayParam.Get(),
                features::DelayCompetingLowPriorityRequestsDelayType::
                    kUseOptimizationGuide);
      ASSERT_EQ(
          features::kDelayCompetingLowPriorityRequestsThresholdParam.Get(),
          features::kDelayCompetingLowPriorityRequestsThresholdParam
              .default_value);
    } else {
      ASSERT_EQ(features::kDelayCompetingLowPriorityRequestsDelayParam.Get(),
                until_);
      ASSERT_EQ(
          features::kDelayCompetingLowPriorityRequestsThresholdParam.Get(),
          priority_threshold_);
    }
    ResourceLoadSchedulerTestBase::SetUp();
  }

  // Returns a ResourceLoadPriority value (either kHigh, or kMedium)
  // corresponding with the priority threshold field trial parameter.
  ResourceLoadPriority ImportantPriority() {
    if (priority_threshold_ ==
        features::DelayCompetingLowPriorityRequestsThreshold::kMedium) {
      return ResourceLoadPriority::kMedium;
    }

    if (priority_threshold_ ==
        features::DelayCompetingLowPriorityRequestsThreshold::kHigh) {
      return ResourceLoadPriority::kHigh;
    }

    NOTREACHED();
    return ResourceLoadPriority::kUnresolved;
  }

 protected:
  features::DelayCompetingLowPriorityRequestsDelayType until_;
  features::DelayCompetingLowPriorityRequestsThreshold priority_threshold_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ResourceLoadSchedulerTestDelayCompetingLowPriorityRequests,
    testing::Combine(
        // True when use optimization guide:
        testing::Bool(),
        // Delay type parameter:
        testing::Values(
            features::DelayCompetingLowPriorityRequestsDelayType::kFirstPaint,
            features::DelayCompetingLowPriorityRequestsDelayType::
                kFirstContentfulPaint),
        // Priority threshold parameter:
        testing::Values(
            features::DelayCompetingLowPriorityRequestsThreshold::kMedium,
            features::DelayCompetingLowPriorityRequestsThreshold::kHigh)));

TEST_P(ResourceLoadSchedulerTestDelayCompetingLowPriorityRequests,
       DelayRequests) {
  ResourceLoadPriority important = ImportantPriority();

  // No throttling, so we can test the delay logic in an unobstructed way.
  Scheduler()->OnLifecycleStateChanged(
      scheduler::SchedulingLifecycleState::kNotThrottled);
  Scheduler()->SetOutstandingLimitForTesting(
      ResourceLoadScheduler::kOutstandingUnlimited);

  MockClient* important_client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id1 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(important_client1, ThrottleOption::kThrottleable,
                       important, 0 /* intra_priority */, &id1);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id1);
  EXPECT_TRUE(important_client1->WasRun());

  MockClient* important_client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id2 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(important_client2, ThrottleOption::kThrottleable,
                       important, 0 /* intra_priority */, &id2);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id2);
  EXPECT_TRUE(important_client2->WasRun());

  MockClient* low_client1 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id3 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(low_client1, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 0 /* intra_priority */,
                       &id3);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id3);
  EXPECT_FALSE(low_client1->WasRun());

  MockClient* low_client2 = MakeGarbageCollected<MockClient>();
  ResourceLoadScheduler::ClientId id4 = ResourceLoadScheduler::kInvalidClientId;
  Scheduler()->Request(low_client2, ThrottleOption::kThrottleable,
                       ResourceLoadPriority::kLow, 0 /* intra_priority */,
                       &id4);
  EXPECT_NE(ResourceLoadScheduler::kInvalidClientId, id4);
  EXPECT_FALSE(low_client2->WasRun());

  EXPECT_TRUE(ReleaseAndSchedule(id1));

  // Releasing one important request is not enough to grant the low priority
  // clients.
  EXPECT_FALSE(low_client1->WasRun());
  EXPECT_FALSE(low_client2->WasRun());

  // Triggering the loading milestone that we're not interested is not enough to
  // grant the low priority clients.
  if (until_ ==
      features::DelayCompetingLowPriorityRequestsDelayType::kFirstPaint) {
    Scheduler()->MarkFirstContentfulPaint();
  } else {
    Scheduler()->MarkFirstPaint();
  }
  EXPECT_FALSE(low_client1->WasRun());
  EXPECT_FALSE(low_client2->WasRun());

  // Triggering the loading milestone we are interested in *is* enough to grant
  // the low priority clients.
  Scheduler()->MarkFirstPaint();
  Scheduler()->MarkFirstContentfulPaint();
  EXPECT_TRUE(low_client1->WasRun());
  EXPECT_TRUE(low_client2->WasRun());

  EXPECT_TRUE(Release(id2));
  EXPECT_TRUE(Release(id3));
  EXPECT_TRUE(Release(id4));

  EXPECT_TRUE(WasDelayCompetingLowPriorityRequestsObserved());
}

}  // namespace
}  // namespace blink
