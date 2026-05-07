// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import "base/task/single_thread_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class TestTool : public actor::ActorTool {
 public:
  TestTool(base::WeakPtr<web::WebState> web_state) : web_state_(web_state) {}
  ~TestTool() override = default;

  void Execute(actor::ToolExecutionCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), actor::ToolExecutionResult::Ok()));
  }

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return web_state_;
  }

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class GeminiActuationHandlerTest : public PlatformTest {
 public:
  GeminiActuationHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    auto fake_web_state = std::make_unique<web::FakeWebState>(
        web::WebStateID::FromSerializedValue(123));
    fake_web_state->SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    fake_web_state->SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    fake_web_state_ = fake_web_state.get();

    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> fake_web_state_;
};

// Tests that a GeminiActuationHandler can be initialized.
TEST_F(GeminiActuationHandlerTest, Initialization) {
  ASSERT_NE(nullptr, actor_service_);
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  EXPECT_NE(nil, handler);
}

// Tests that createTaskWithTitle returns a valid task ID.
TEST_F(GeminiActuationHandlerTest, CreateTask) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];
  EXPECT_FALSE(task_id.is_null());
}

// Tests that performActions returns a failure result when passed an invalid
// serialized proto.
TEST_F(GeminiActuationHandlerTest, PerformActions_InvalidProto) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  NSData* invalid_data = [@"invalid" dataUsingEncoding:NSUTF8StringEncoding];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    EXPECT_NE(nil, serializedActionsResult);
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(actions_result.action_result(), actor::kActionResultFailure);
      EXPECT_EQ(actions_result.error_message(), "Failed to parse action proto");
    }
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ invalid_data ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions succeeds with an empty list of protos.
TEST_F(GeminiActuationHandlerTest, PerformActions_EmptyProtos) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(actions_result.action_result(), 0);
      EXPECT_EQ(actions_result.tabs_size(), 0);
    }
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions can parse multiple valid protos.
TEST_F(GeminiActuationHandlerTest, PerformActions_MultipleProtos) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action1;
  std::string serialized1;
  action1.SerializeToString(&serialized1);
  NSData* data1 = [NSData dataWithBytes:serialized1.data()
                                 length:serialized1.size()];

  optimization_guide::proto::Action action2;
  std::string serialized2;
  action2.SerializeToString(&serialized2);
  NSData* data2 = [NSData dataWithBytes:serialized2.data()
                                 length:serialized2.size()];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_NE(actions_result.action_result(), 0);
    }
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data1, data2 ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that pauseTaskWithID does not crash.
TEST_F(GeminiActuationHandlerTest, PauseTask) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  [handler pauseTaskWithID:task_id];
}

// Tests that stopTaskWithID does not crash.
TEST_F(GeminiActuationHandlerTest, StopTask) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  [handler stopTaskWithID:task_id
                   reason:actor::ActorTaskStoppedReason::kTaskComplete];
}

// Tests that requestActionablePageContextForWebStateIDs returns
// TAB_OBSERVATION_TAB_WENT_AWAY when tab is not found.
TEST_F(GeminiActuationHandlerTest, RequestActionablePageContext_NotFound) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSArray<NSData*>* serializedTabObservations) {
    callback_called = YES;
    EXPECT_EQ([serializedTabObservations count], 1u);
    if ([serializedTabObservations count] > 0) {
      optimization_guide::proto::TabObservation tabObservation;
      EXPECT_TRUE(
          tabObservation.ParseFromArray([serializedTabObservations[0] bytes],
                                        [serializedTabObservations[0] length]));
      EXPECT_EQ(tabObservation.result(),
                optimization_guide::proto::TabObservation::
                    TAB_OBSERVATION_TAB_WENT_AWAY);
    }
  };

  [handler requestActionablePageContextForWebStateIDs:@[ @999 ]
                                               taskID:task_id
                                      completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that requestActionablePageContextForWebStateIDs returns errors for all
// missing tabs.
TEST_F(GeminiActuationHandlerTest,
       RequestActionablePageContext_MultipleNotFound) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSArray<NSData*>* serializedTabObservations) {
    callback_called = YES;
    EXPECT_EQ([serializedTabObservations count], 2u);
    for (NSData* data in serializedTabObservations) {
      optimization_guide::proto::TabObservation tabObservation;
      EXPECT_TRUE(tabObservation.ParseFromArray([data bytes], [data length]));
      EXPECT_EQ(tabObservation.result(),
                optimization_guide::proto::TabObservation::
                    TAB_OBSERVATION_TAB_WENT_AWAY);
    }
  };

  [handler requestActionablePageContextForWebStateIDs:@[ @999, @888 ]
                                               taskID:task_id
                                      completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions correctly injects the active tab ID into an action
// proto lacking tab_id.
TEST_F(GeminiActuationHandlerTest, PerformActions_InjectsTabId) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];

  // Activate the fake web state (index 0).
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  // Create a ClickAction proto without a tab_id.
  optimization_guide::proto::Action action;
  auto* click = action.mutable_click();
  click->mutable_target()->mutable_coordinate()->set_x(50);
  click->mutable_target()->mutable_coordinate()->set_y(50);
  click->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    ASSERT_NE(nil, serializedActionsResult);
    optimization_guide::proto::ActionsResult actions_result;
    EXPECT_TRUE(actions_result.ParseFromArray(
        [serializedActionsResult bytes], [serializedActionsResult length]));

    // If the tab ID injection succeeded, the Actor Tool Factory will
    // successfully resolve the tab (finding our active fake WebState with ID
    // 123), and not failing with an invalid arguments error due to a missing
    // tab ID.
    std::string invalid_args_error =
        actor::GetToolExecutionResultMessage(actor::ToolExecutionResult(
            actor::mojom::ActionResultCode::kArgumentsInvalid));
    EXPECT_NE(actions_result.error_message(), invalid_args_error);
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions returns a failure result when passed an unmapped
// task ID.
TEST_F(GeminiActuationHandlerTest, PerformActions_UnmappedTaskId) {
  GeminiActuationHandler* handler = [[GeminiActuationHandler alloc]
      initWithActorService:actor_service_
              webStateList:browser_->GetWebStateList()];

  // Generate an arbitrary task ID that is not mapped.
  actor::ActorTaskId unmapped_task_id = actor::ActorTaskId();

  optimization_guide::proto::Action action;
  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    ASSERT_NE(nil, serializedActionsResult);
    optimization_guide::proto::ActionsResult actions_result;
    EXPECT_TRUE(actions_result.ParseFromArray(
        [serializedActionsResult bytes], [serializedActionsResult length]));
    EXPECT_EQ(actions_result.action_result(), actor::kActionResultFailure);
    EXPECT_EQ(actions_result.error_message(),
              "Failed to perform actions: Task ID not found.");
  };

  [handler performActionsWithTaskID:unmapped_task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

}  // namespace
