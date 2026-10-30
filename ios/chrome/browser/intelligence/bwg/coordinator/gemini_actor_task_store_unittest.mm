// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/actor/core/task_source_info.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// Records the worklog published by an `ActuationWorklogDataSource`.
@interface FakeWorklogDataSourceObserver
    : NSObject <ActuationWorklogDataSourceObserver>
@property(nonatomic, copy) NSString* taskTitle;
@property(nonatomic, assign) actor::ActorTaskState taskState;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogItem*>* items;
@property(nonatomic, assign) std::optional<actor::ToolType> latestToolType;
@property(nonatomic, assign) NSUInteger attachCount;
@end

@implementation FakeWorklogDataSourceObserver

- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
    didAttachWithTaskTitle:(NSString*)taskTitle
                 taskState:(actor::ActorTaskState)taskState
                     items:(NSArray<ActuationWorklogItem*>*)items
            latestToolType:(std::optional<actor::ToolType>)latestToolType {
  _attachCount++;
  _taskTitle = [taskTitle copy];
  _taskState = taskState;
  _items = [items mutableCopy];
  _latestToolType = latestToolType;
}

- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
       didUpdateTaskTitle:(NSString*)taskTitle
                taskState:(actor::ActorTaskState)taskState {
  _taskTitle = [taskTitle copy];
  _taskState = taskState;
}

- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
            didAppendItem:(ActuationWorklogItem*)item
                 toolType:(std::optional<actor::ToolType>)toolType {
  [_items addObject:item];
  _latestToolType = toolType;
}

@end

namespace {

using enum actor::ActorTaskState;
using enum actor::ToolType;

// Title of the tasks created by the tests.
constexpr char kTaskTitle[] = "Task Title";

// Returns the provenance attributed to tasks created by these tests.
actor::TaskSourceInfo TestSource() {
  return actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                               /*id=*/std::nullopt);
}

class GeminiActorTaskStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(actor_service_);
    task_id_ = CreateTask();
    store_ = [[GeminiActorTaskStore alloc] initWithTaskID:task_id_
                                             actorService:actor_service_];
    observer_ = [[FakeWorklogDataSourceObserver alloc] init];
  }

  void TearDown() override {
    [store_ disconnect];
    store_ = nil;
    actor_service_ = nullptr;
    PlatformTest::TearDown();
  }

  actor::ActorTaskId CreateTask() {
    return actor_service_->CreateTask(kTaskTitle, TestSource(),
                                      /*allow_incognito_web_states=*/false);
  }

  // Adds `observer_` and delivers the registration posted by the task.
  void AttachAndRegister() {
    [store_ addObserver:observer_];
    actor::FlushCurrentSequence();
  }

  void ExecuteTool(actor::ToolType tool_type, NSString* update) {
    [store_ actorTaskWithID:task_id_
            willExecuteTool:tool_type
                 taskUpdate:update
                 onWebState:web::WebStateID::FromSerializedValue(1)];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  actor::ActorTaskId task_id_;
  GeminiActorTaskStore* store_;
  FakeWorklogDataSourceObserver* observer_;
};

// Tests that an observer attached before the registration receives an empty
// worklog, then the task title and the Gemini first step.
TEST_F(GeminiActorTaskStoreTest, RegistrationPublishesTitleAndFirstStep) {
  [store_ addObserver:observer_];
  EXPECT_EQ(observer_.items.count, 0u);

  actor::FlushCurrentSequence();

  EXPECT_NSEQ(observer_.taskTitle, @(kTaskTitle));
  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(
      observer_.items[0].title,
      l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE));
  EXPECT_FALSE(observer_.latestToolType.has_value());
}

// Tests that a non-empty initial task update is appended after the Gemini first
// step.
TEST_F(GeminiActorTaskStoreTest, InitialTaskUpdateFollowsFirstStep) {
  [store_ addObserver:observer_];

  [store_ didRegisterAsObserverForTaskID:task_id_
                               taskTitle:@(kTaskTitle)
                              taskUpdate:@"Initial update"
                            currentState:kActing
                               webStates:@[]];

  ASSERT_EQ(observer_.items.count, 2u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(observer_.items[1].title, @"Initial update");
}

// Tests that tool executions append items with their tool.
TEST_F(GeminiActorTaskStoreTest, ToolExecutionAppendsItem) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;

  ExecuteTool(kClick, @"Clicking button");

  ASSERT_EQ(observer_.items.count, initialCount + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title, @"Clicking button");
  EXPECT_EQ(observer_.items.lastObject.style,
            ActuationWorklogItemStyle::kSimple);
  EXPECT_EQ(observer_.latestToolType, kClick);
}

// Tests that empty and consecutive duplicate task updates are ignored.
TEST_F(GeminiActorTaskStoreTest, IgnoresEmptyAndDuplicateUpdates) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;

  ExecuteTool(kType, @"");
  EXPECT_EQ(observer_.items.count, initialCount);

  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 1);

  ExecuteTool(kScroll, @"Step B");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 3);
}

// Tests that state changes are published.
TEST_F(GeminiActorTaskStoreTest, StateChangeIsPublished) {
  AttachAndRegister();

  [store_ actorTaskWithID:task_id_
           didChangeState:kPausedByUser
                fromState:kActing];

  EXPECT_EQ(observer_.taskState, kPausedByUser);
  EXPECT_NSEQ(observer_.taskTitle, @(kTaskTitle));
}

// Tests that an observer added mid-task receives the whole worklog.
TEST_F(GeminiActorTaskStoreTest, LateObserverReceivesWorklog) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;
  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kNavigate, @"Step B");
  [store_ actorTaskWithID:task_id_
           didChangeState:kPausedByUser
                fromState:kActing];

  FakeWorklogDataSourceObserver* lateObserver =
      [[FakeWorklogDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];

  EXPECT_EQ(lateObserver.attachCount, 1u);
  EXPECT_NSEQ(lateObserver.taskTitle, @(kTaskTitle));
  EXPECT_EQ(lateObserver.taskState, kPausedByUser);
  ASSERT_EQ(lateObserver.items.count, initialCount + 2);
  EXPECT_NSEQ(lateObserver.items.lastObject.title, @"Step B");
  EXPECT_EQ(lateObserver.latestToolType, kNavigate);
}

// Tests that removed observers stop receiving updates.
TEST_F(GeminiActorTaskStoreTest, RemovedObserverStopsReceivingUpdates) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;
  [store_ removeObserver:observer_];

  ExecuteTool(kClick, @"Step A");

  EXPECT_EQ(observer_.items.count, initialCount);
}

// Tests that adding an observer twice only sends it the worklog once.
TEST_F(GeminiActorTaskStoreTest, AddingObserverTwiceSendsWorklogOnce) {
  AttachAndRegister();

  [store_ addObserver:observer_];

  EXPECT_EQ(observer_.attachCount, 1u);
}

}  // namespace
