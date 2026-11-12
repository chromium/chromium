// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store_manager.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "components/actor/core/task_source_info.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_lifecycle_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface GeminiActorTaskStoreManager (Testing) <ActorTaskLifecycleObserver>
@end

// Records the stores created and disconnected by the manager.
@interface FakeGeminiActorTaskStoreManagerObserver
    : NSObject <GeminiActorTaskStoreManagerObserver>
@property(nonatomic, readonly) NSArray<GeminiActorTaskStore*>* createdStores;
@property(nonatomic, readonly)
    NSArray<GeminiActorTaskStore*>* disconnectedStores;
// Whether the latest disconnected store still had a non-nil `state` when
// `didDisconnectStore:` ran.
@property(nonatomic, readonly) BOOL lastDisconnectedStoreHadState;
@end

@implementation FakeGeminiActorTaskStoreManagerObserver {
  NSMutableArray<GeminiActorTaskStore*>* _createdStores;
  NSMutableArray<GeminiActorTaskStore*>* _disconnectedStores;
}

- (instancetype)init {
  if ((self = [super init])) {
    _createdStores = [NSMutableArray array];
    _disconnectedStores = [NSMutableArray array];
  }
  return self;
}

- (NSArray<GeminiActorTaskStore*>*)createdStores {
  return _createdStores;
}

- (NSArray<GeminiActorTaskStore*>*)disconnectedStores {
  return _disconnectedStores;
}

- (void)actorTaskStoreManager:(GeminiActorTaskStoreManager*)manager
               didCreateStore:(GeminiActorTaskStore*)store {
  [_createdStores addObject:store];
}

- (void)actorTaskStoreManager:(GeminiActorTaskStoreManager*)manager
           didDisconnectStore:(GeminiActorTaskStore*)store {
  _lastDisconnectedStoreHadState = (store.state != nil);
  [_disconnectedStores addObject:store];
}

@end

namespace {

constexpr actor::ActorTaskId kTaskId = actor::ActorTaskId(1);
constexpr actor::ActorTaskId kOtherTaskId = actor::ActorTaskId(2);

// Title of the tasks created on the `ActorService`.
constexpr char kTaskTitle[] = "Task Title";

// Returns the source of a task started by `client`.
actor::TaskSourceInfo SourceFor(actor::TaskSourceInfo::Client client) {
  return actor::TaskSourceInfo(client, /*id=*/std::nullopt);
}

// Tests the manager without an `ActorService`, by sending it the task
// lifecycle events directly.
class GeminiActorTaskStoreManagerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    manager_ =
        [[GeminiActorTaskStoreManager alloc] initWithActorService:nullptr];
    observer_ = [[FakeGeminiActorTaskStoreManagerObserver alloc] init];
    [manager_ addObserver:observer_];
  }

  void TearDown() override {
    [manager_ disconnect];
    PlatformTest::TearDown();
  }

  void StartTask(actor::ActorTaskId task_id,
                 actor::TaskSourceInfo::Client client =
                     actor::TaskSourceInfo::Client::kGlic) {
    [manager_ actorServiceDidStartTaskWithID:task_id
                                  sourceInfo:SourceFor(client)];
  }

  void StopTask(actor::ActorTaskId task_id) {
    [manager_
        actorServiceDidStopTaskWithID:task_id
                           sourceInfo:SourceFor(
                                          actor::TaskSourceInfo::Client::kGlic)
                           finalState:actor::ActorTaskState::kFinished];
  }

  GeminiActorTaskStoreManager* manager_;
  FakeGeminiActorTaskStoreManagerObserver* observer_;
};

// Tests that starting and stopping a Gemini task notifies observers.
TEST_F(GeminiActorTaskStoreManagerTest, TaskLifecycle) {
  StartTask(kTaskId);
  ASSERT_EQ(observer_.createdStores.count, 1u);
  EXPECT_EQ(observer_.createdStores[0].taskID, kTaskId);

  StopTask(kTaskId);
  ASSERT_EQ(observer_.disconnectedStores.count, 1u);
  EXPECT_EQ(observer_.disconnectedStores[0], observer_.createdStores[0]);
}

// Tests that non-Gemini tasks are ignored.
TEST_F(GeminiActorTaskStoreManagerTest, IgnoresNonGeminiTasks) {
  StartTask(kTaskId, actor::TaskSourceInfo::Client::kTtc);
  EXPECT_EQ(observer_.createdStores.count, 0u);
}

// Tests that stops of tasks other than the active one are ignored.
TEST_F(GeminiActorTaskStoreManagerTest, IgnoresStopOfOtherTask) {
  StartTask(kTaskId);
  StopTask(kOtherTaskId);
  EXPECT_EQ(observer_.disconnectedStores.count, 0u);
}

// Tests that a new Gemini task replaces the active store.
TEST_F(GeminiActorTaskStoreManagerTest, NewTaskReplacesActiveStore) {
  StartTask(kTaskId);
  StartTask(kOtherTaskId);
  ASSERT_EQ(observer_.createdStores.count, 2u);
  EXPECT_EQ(observer_.createdStores[1].taskID, kOtherTaskId);
  EXPECT_EQ(observer_.disconnectedStores.count, 0u);

  StopTask(kTaskId);
  EXPECT_EQ(observer_.disconnectedStores.count, 0u);

  StopTask(kOtherTaskId);
  ASSERT_EQ(observer_.disconnectedStores.count, 1u);
  EXPECT_EQ(observer_.disconnectedStores[0], observer_.createdStores[1]);
}

// Tests the manager with a real `ActorService`, which posts the task lifecycle
// events.
class GeminiActorTaskStoreManagerServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(actor_service_);
    manager_ = [[GeminiActorTaskStoreManager alloc]
        initWithActorService:actor_service_];
    observer_ = [[FakeGeminiActorTaskStoreManagerObserver alloc] init];
    [manager_ addObserver:observer_];
    // Deliver the posted lifecycle observer registration.
    actor::FlushCurrentSequence();
  }

  void TearDown() override {
    [manager_ disconnect];
    actor_service_ = nullptr;
    PlatformTest::TearDown();
  }

  actor::ActorTaskId CreateGeminiTask() {
    return actor_service_->CreateTask(
        kTaskTitle, SourceFor(actor::TaskSourceInfo::Client::kGlic),
        /*allow_incognito_web_states=*/false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  GeminiActorTaskStoreManager* manager_;
  FakeGeminiActorTaskStoreManagerObserver* observer_;
};

// Tests that the manager relays the start and stop of a Gemini task started on
// the service, disconnecting the store before `didDisconnectStore:`.
TEST_F(GeminiActorTaskStoreManagerServiceTest, TaskLifecycle) {
  const actor::ActorTaskId task_id = CreateGeminiTask();
  actor::FlushCurrentSequence();
  ASSERT_EQ(observer_.createdStores.count, 1u);
  EXPECT_EQ(observer_.createdStores[0].taskID, task_id);
  ASSERT_TRUE(observer_.createdStores[0].state);

  actor_service_->StopTask(task_id,
                           actor::ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return observer_.disconnectedStores.count > 0; }));
  EXPECT_EQ(observer_.disconnectedStores[0], observer_.createdStores[0]);
  EXPECT_FALSE(observer_.lastDisconnectedStoreHadState);
}

// Tests that disconnecting the manager disconnects the active store without
// calling `didDisconnectStore:` and stops relaying tasks.
TEST_F(GeminiActorTaskStoreManagerServiceTest, DisconnectStopsRelaying) {
  CreateGeminiTask();
  actor::FlushCurrentSequence();
  ASSERT_EQ(observer_.createdStores.count, 1u);
  ASSERT_TRUE(observer_.createdStores[0].state);

  [manager_ disconnect];
  EXPECT_FALSE(observer_.createdStores[0].state);
  EXPECT_EQ(observer_.disconnectedStores.count, 0u);

  CreateGeminiTask();
  actor::FlushCurrentSequence();
  EXPECT_EQ(observer_.createdStores.count, 1u);
}

}  // namespace
