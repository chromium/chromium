// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
constexpr actor::ActorTaskId kTaskId = actor::ActorTaskId(1);
using enum actor::ActorTaskState;
using enum actor::ToolType;
}  // namespace

@interface FakeActuationWorklogConsumer : NSObject <ActuationWorklogConsumer>
@property(nonatomic, copy) NSString* taskTitle;
@property(nonatomic, assign) BOOL actuationActive;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogItem*>* items;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogChip*>* chips;
@end

@implementation FakeActuationWorklogConsumer

- (instancetype)init {
  if ((self = [super init])) {
    _items = [NSMutableArray array];
    _chips = [NSMutableArray array];
  }
  return self;
}

- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated {
  if (item) {
    [_items addObject:item];
  }
  if (chip) {
    [_chips addObject:chip];
  }
}

- (void)reset {
  [_items removeAllObjects];
  [_chips removeAllObjects];
  _taskTitle = nil;
}

@end

class ActuationWorklogMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    fake_consumer_ = [[FakeActuationWorklogConsumer alloc] init];
    mediator_ = [[ActuationWorklogMediator alloc] init];
    mediator_.consumer = fake_consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    PlatformTest::TearDown();
  }

  void RegisterTask(NSString* title = @"Task Title",
                    NSString* update = nil,
                    actor::ActorTaskState state = kActing) {
    [mediator_ didRegisterAsObserverForTaskID:kTaskId
                                    taskTitle:title
                                   taskUpdate:update
                                 currentState:state
                                    webStates:@[]];
  }

  void ChangeState(actor::ActorTaskState new_state,
                   actor::ActorTaskState old_state = kActing) {
    [mediator_ actorTaskWithID:kTaskId
                didChangeState:new_state
                     fromState:old_state];
  }

  void EndActuation(actor::ActorTaskState final_state = kFinished) {
    [mediator_ actorTaskDidStopWithID:kTaskId finalState:final_state];
  }

  void ExecuteTool(actor::ToolType tool_type, NSString* update = @"") {
    [mediator_ actorTaskWithID:kTaskId
               willExecuteTool:tool_type
                    taskUpdate:update
                    onWebState:web::WebStateID::FromSerializedValue(1)];
  }

  FakeActuationWorklogConsumer* fake_consumer_;
  ActuationWorklogMediator* mediator_;
};

// Tests observer registration forwards the title and emits the initial step.
TEST_F(ActuationWorklogMediatorTest, TestRegisterObserverEmitsInitialStep) {
  RegisterTask();

  EXPECT_NSEQ(fake_consumer_.taskTitle, @"Task Title");
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_GT(fake_consumer_.items[0].title.length, 0u);
  EXPECT_GT(fake_consumer_.items[0].subtitle.length, 0u);
  EXPECT_TRUE(fake_consumer_.items[0].active);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);
}

// Tests that tool execution emits a chip and timeline item.
TEST_F(ActuationWorklogMediatorTest, TestToolExecutionEmitsChipAndItem) {
  ChangeState(kActing, kInit);

  // Empty updates are ignored.
  ExecuteTool(kType, @"");
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);

  ExecuteTool(kClick, @"Clicking button");
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, @"Clicking button");
  ASSERT_EQ(fake_consumer_.chips.count, 1u);
  EXPECT_GT(fake_consumer_.chips[0].text.length, 0u);
}

// Tests that consecutive duplicate task updates are deduplicated.
TEST_F(ActuationWorklogMediatorTest, TestConsecutiveDeduplication) {
  ChangeState(kActing, kInit);
  ExecuteTool(kClick, @"Action A");
  ExecuteTool(kScroll, @"Action A");
  ExecuteTool(kType, @"Action B");
  ExecuteTool(kClick, @"Action A");

  ASSERT_EQ(fake_consumer_.items.count, 3u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, @"Action A");
  EXPECT_NSEQ(fake_consumer_.items[1].title, @"Action B");
  EXPECT_NSEQ(fake_consumer_.items[2].title, @"Action A");
}

// Tests the different state transitions.
TEST_F(ActuationWorklogMediatorTest, TestTaskLifecycleTransitionsAndReset) {
  EXPECT_FALSE(fake_consumer_.actuationActive);

  RegisterTask();
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.taskTitle, nil);

  ChangeState(kReflecting);
  EXPECT_TRUE(fake_consumer_.actuationActive);

  EndActuation();
  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.taskTitle, nil);
}

// Tests that disconnecting the mediator resets the consumer.
TEST_F(ActuationWorklogMediatorTest, TestDisconnectResetsConsumer) {
  RegisterTask();
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.taskTitle, nil);

  [mediator_ disconnect];
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.taskTitle, nil);
}
