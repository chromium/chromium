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
@property(nonatomic, strong) ActuationInterventionData* intervention;
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

- (void)setIntervention:(ActuationInterventionData*)intervention {
  _intervention = intervention;
}

- (void)reset {
  [_items removeAllObjects];
  [_chips removeAllObjects];
  _taskTitle = nil;
  _intervention = nil;
}

@end

class ActuationWorklogMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    fake_consumer_ = [[FakeActuationWorklogConsumer alloc] init];
    mediator_ = [[ActuationWorklogMediator alloc] initWithActorService:nullptr];
    mediator_.consumer = fake_consumer_;
    [mediator_ connect];
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
  RegisterTask();
  [fake_consumer_.items removeAllObjects];

  // Empty updates are ignored.
  ExecuteTool(kType, @"");
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);

  ExecuteTool(kClick, @"Clicking button");
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, @"Clicking button");
  ASSERT_EQ(fake_consumer_.chips.count, 1u);
  EXPECT_GT(fake_consumer_.chips[0].text.length, 0u);

  ExecuteTool(kNavigate, @"Navigating to page");
  ASSERT_EQ(fake_consumer_.items.count, 2u);
  EXPECT_NSEQ(fake_consumer_.items[1].title, @"Navigating to page");
  ASSERT_EQ(fake_consumer_.chips.count, 2u);
  EXPECT_GT(fake_consumer_.chips[1].text.length, 0u);

  // Unmapped tools emit the catch-all "Processing" chip.
  ExecuteTool(kSelect, @"Selecting item");
  ASSERT_EQ(fake_consumer_.items.count, 3u);
  EXPECT_NSEQ(fake_consumer_.items[2].title, @"Selecting item");
  ASSERT_EQ(fake_consumer_.chips.count, 3u);
  EXPECT_NSEQ(fake_consumer_.chips[2].text, @"Processing");

  // Tools without chips (e.g. kWaitZeroDuration) do not emit a chip.
  ExecuteTool(kWaitZeroDuration, @"Stabilizing");
  ASSERT_EQ(fake_consumer_.items.count, 4u);
  EXPECT_NSEQ(fake_consumer_.items[3].title, @"Stabilizing");
  EXPECT_EQ(fake_consumer_.chips.count, 3u);
}

// Tests that consecutive duplicate task updates are deduplicated.
TEST_F(ActuationWorklogMediatorTest, TestConsecutiveDeduplication) {
  RegisterTask();
  [fake_consumer_.items removeAllObjects];

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

// Tests the user intervention flow.
TEST_F(ActuationWorklogMediatorTest, TestUserInterventionFlow) {
  RegisterTask();
  __block BOOL completion_called = NO;
  [mediator_ actorTask:kTaskId
      requestUserInterventionWithTitle:@"Intervention Title"
                              subtitle:@"Intervention Subtitle"
                            buttonText:@"Continue"
                     completionHandler:^{
                       completion_called = YES;
                     }];

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @"Intervention Title");
  EXPECT_NSEQ(fake_consumer_.intervention.subtitle, @"Intervention Subtitle");
  EXPECT_NSEQ(fake_consumer_.intervention.primaryButtonText, @"Continue");
  EXPECT_FALSE(completion_called);

  [mediator_ didTapInterventionButton];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that cancelling, replacing, or disconnecting an intervention discards
// the pending completion without invoking it.
TEST_F(ActuationWorklogMediatorTest, TestUserInterventionCancellation) {
  RegisterTask();
  __block BOOL first_completion_called = NO;
  [mediator_ actorTask:kTaskId
      requestUserInterventionWithTitle:@"First Title"
                              subtitle:@"First Subtitle"
                            buttonText:@"Action 1"
                     completionHandler:^{
                       first_completion_called = YES;
                     }];

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_FALSE(first_completion_called);

  // A second intervention arrives before user acts on the first.
  __block BOOL second_completion_called = NO;
  [mediator_ actorTask:kTaskId
      requestUserInterventionWithTitle:@"Second Title"
                              subtitle:@"Second Subtitle"
                            buttonText:@"Action 2"
                     completionHandler:^{
                       second_completion_called = YES;
                     }];

  // First completion must be discarded without invocation.
  EXPECT_FALSE(first_completion_called);
  EXPECT_FALSE(second_completion_called);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @"Second Title");

  // Task stoppage discards pending completion and clears consumer.
  EndActuation();
  EXPECT_FALSE(second_completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);

  // A third intervention followed by disconnect should also discard completion.
  RegisterTask();
  __block BOOL third_completion_called = NO;
  [mediator_ actorTask:kTaskId
      requestUserInterventionWithTitle:@"Third Title"
                              subtitle:@"Third Subtitle"
                            buttonText:@"Action 3"
                     completionHandler:^{
                       third_completion_called = YES;
                     }];
  ASSERT_NE(fake_consumer_.intervention, nil);
  [mediator_ disconnect];
  EXPECT_FALSE(third_completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}
