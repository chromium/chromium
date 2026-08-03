// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tool_controller.h"

#import <optional>

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

// A test tool that completes synchronously and tracks whether it was destroyed
// before it finished executing, to verify UAF protection.
class SyncActorTool : public ActorTool {
 public:
  SyncActorTool(bool* tool_destroyed_flag, bool* callback_completed_flag)
      : tool_destroyed_flag_(tool_destroyed_flag),
        callback_completed_flag_(callback_completed_flag) {}

  ~SyncActorTool() override {
    if (tool_destroyed_flag_) {
      *tool_destroyed_flag_ = true;
    }
  }

  void Validate(ToolExecutionCallback callback) override {
    std::move(callback).Run(ToolExecutionResult::Ok());
  }

  void Execute(ToolExecutionCallback callback) override {
    // Copy members to local variables on the stack BEFORE running the callback,
    // so we don't access 'this' members if we are deleted inside the callback.
    bool* callback_completed_flag = callback_completed_flag_;
    bool* tool_destroyed_flag = tool_destroyed_flag_;

    std::move(callback).Run(ToolExecutionResult::Ok());

    if (tool_destroyed_flag && *tool_destroyed_flag) {
      if (callback_completed_flag) {
        *callback_completed_flag = false;
      }
    } else {
      if (callback_completed_flag) {
        *callback_completed_flag = true;
      }
    }
  }

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return nullptr;
  }
  ToolType GetToolType() const override { return ToolType::kWait; }

 private:
  bool* tool_destroyed_flag_;
  bool* callback_completed_flag_;
};

class SyncActorToolFactory : public ActorToolFactory {
 public:
  explicit SyncActorToolFactory(ProfileIOS* profile,
                                bool* tool_destroyed_flag,
                                bool* callback_completed_flag)
      : ActorToolFactory(profile),
        tool_destroyed_flag_(tool_destroyed_flag),
        callback_completed_flag_(callback_completed_flag) {}

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> CreateTool(
      const ActorToolRequest& request,
      ToolDelegate* tool_delegate) override {
    return std::make_unique<SyncActorTool>(tool_destroyed_flag_,
                                           callback_completed_flag_);
  }

 private:
  bool* tool_destroyed_flag_;
  bool* callback_completed_flag_;
};

// A test tool that never completes, added to test ToolController::Cancel.
class AsyncActorTool : public ActorTool {
 public:
  void Validate(ToolExecutionCallback callback) override {
    std::move(callback).Run(ToolExecutionResult::Ok());
  }
  void Execute(ToolExecutionCallback callback) override {
    // Do not run the callback, simulating an async operation that gets
    // cancelled.
  }
  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return nullptr;
  }
  ToolType GetToolType() const override { return ToolType::kWait; }
};

class AsyncActorToolFactory : public ActorToolFactory {
 public:
  explicit AsyncActorToolFactory(ProfileIOS* profile)
      : ActorToolFactory(profile) {}
  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> CreateTool(
      const ActorToolRequest& request,
      ToolDelegate* tool_delegate) override {
    return std::make_unique<AsyncActorTool>();
  }
};

// A test tool that completes by requiring page stabilization.
class StabilizingActorTool : public ActorTool {
 public:
  void Validate(ToolExecutionCallback callback) override {
    std::move(callback).Run(ToolExecutionResult::Ok());
  }
  void Execute(ToolExecutionCallback callback) override {
    std::move(callback).Run(ToolExecutionResult(
        mojom::ActionResultCode::kOk, /*requires_page_stabilization=*/true));
  }
  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return web_state_;
  }
  ToolType GetToolType() const override { return ToolType::kWait; }

  void SetWebState(base::WeakPtr<web::WebState> web_state) {
    web_state_ = web_state;
  }

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class StabilizingActorToolFactory : public ActorToolFactory {
 public:
  explicit StabilizingActorToolFactory(ProfileIOS* profile,
                                       base::WeakPtr<web::WebState> web_state)
      : ActorToolFactory(profile), web_state_(web_state) {}

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> CreateTool(
      const ActorToolRequest& request,
      ToolDelegate* tool_delegate) override {
    auto tool = std::make_unique<StabilizingActorTool>();
    tool->SetWebState(web_state_);
    return tool;
  }

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class ToolControllerTest : public PlatformTest, public ToolDelegate {
 protected:
  ToolControllerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    journal_ = std::make_unique<AggregatedJournal>();
    tool_factory_ = std::make_unique<ActorToolFactory>(profile_.get());
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = std::make_unique<ToolController>(this);
  }

  // ToolDelegate overrides.
  ActorTaskId GetTaskId() const override { return ActorTaskId(1); }
  bool IsWindowIdValid(int32_t window_id) override { return false; }
  web::WebState* InsertWebState(
      int32_t window_id,
      const web::NavigationManager::WebLoadParams& load_params,
      bool in_background) override {
    return nullptr;
  }
  AggregatedJournal& GetJournal() const override { return *journal_; }
  ActorToolFactory& GetToolFactory() const override { return *tool_factory_; }
  ActorTaskFormFillingHandler* GetActorTaskFormFillingHandler() override {
    return nullptr;
  }
  void InterruptFromTool() override {}
  void UninterruptFromTool() override {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ActorToolFactory> tool_factory_;
  std::unique_ptr<AggregatedJournal> journal_;
  std::unique_ptr<ToolController> controller_;
};

// Tests that a tool can be created, validated, and invoked successfully.
TEST_F(ToolControllerTest, SuccessfulExecutionFlow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  base::RunLoop run_loop;
  bool callback_called = false;
  std::optional<ToolExecutionResult> validation_result;

  controller_->CreateToolAndValidate(
      *request, base::BindOnce(
                    [](bool* called, std::optional<ToolExecutionResult>* out,
                       base::RunLoop* loop, ToolExecutionResult result) {
                      *called = true;
                      *out = result;
                      loop->Quit();
                    },
                    &callback_called, &validation_result, &run_loop));

  run_loop.Run();
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(validation_result.has_value());
  EXPECT_TRUE(validation_result->IsOk());

  base::RunLoop run_loop2;
  bool invoke_callback_called = false;
  std::optional<ToolExecutionResult> invoke_result;

  controller_->Invoke(base::BindOnce(
      [](bool* called, std::optional<ToolExecutionResult>* out,
         base::RunLoop* loop, ToolExecutionResult result) {
        *called = true;
        *out = result;
        loop->Quit();
      },
      &invoke_callback_called, &invoke_result, &run_loop2));

  run_loop2.Run();
  EXPECT_TRUE(invoke_callback_called);
  ASSERT_TRUE(invoke_result.has_value());
  EXPECT_TRUE(invoke_result->IsOk());
}

// Tests that if tool validation fails the ToolController returns the error.
TEST_F(ToolControllerTest, ValidationFailure) {
  // A request that will pass creation but fail validation.
  std::unique_ptr<ActorToolRequest> request = MakeFailingActorToolRequest();

  base::test::TestFuture<ToolExecutionResult> future;
  controller_->CreateToolAndValidate(*request, future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Tests that if tool creation fails synchronously, it transitions back to READY
// and doesn't crash the state machine during Cancel.
TEST_F(ToolControllerTest, SyncCreationFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  // An invalid request that will fail tool creation
  optimization_guide::proto::Action action;
  // action has action_case == ACTION_NOT_SET, which will fail creation
  ActorToolRequest request(action);

  base::RunLoop run_loop;
  bool callback_called = false;
  std::optional<ToolExecutionResult> creation_result;

  controller_->CreateToolAndValidate(
      request, base::BindOnce(
                   [](bool* called, std::optional<ToolExecutionResult>* out,
                      base::RunLoop* loop, ToolExecutionResult result) {
                     *called = true;
                     *out = result;
                     loop->Quit();
                   },
                   &callback_called, &creation_result, &run_loop));

  run_loop.Run();
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(creation_result.has_value());
  EXPECT_FALSE(creation_result->IsOk());

  // Cancel should be a no-op / safe transition.
  controller_->Cancel();
}

// Tests that a tool can be canceled during execution.
TEST_F(ToolControllerTest, CancelMidExecution) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  tool_factory_ = std::make_unique<AsyncActorToolFactory>(profile_.get());

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  base::test::TestFuture<ToolExecutionResult> validation_future;
  controller_->CreateToolAndValidate(*request, validation_future.GetCallback());
  EXPECT_TRUE(validation_future.Get().IsOk());

  controller_->Invoke(base::BindOnce([](ToolExecutionResult result) {
    FAIL() << "Callback should not be called when cancelled.";
  }));

  controller_->Cancel();
}

// Tests that if a navigation occurs during the observation delay, the
// observation is restarted.
TEST_F(ToolControllerTest, RestartObservationOnNavigation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kActorTools, {{"PageStabilityEnabled", "true"}});
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetLoading(true);

  tool_factory_ = std::make_unique<StabilizingActorToolFactory>(
      profile_.get(), fake_web_state->GetWeakPtr());
  controller_ = std::make_unique<ToolController>(this);

  base::test::TestFuture<ToolExecutionResult> validation_future;
  controller_->CreateToolAndValidate(*MakeSuccessfulActorToolRequest(),
                                     validation_future.GetCallback());
  EXPECT_TRUE(validation_future.Get().IsOk());

  base::test::TestFuture<ToolExecutionResult> invoke_future;
  controller_->Invoke(invoke_future.GetCallback());
  EXPECT_FALSE(invoke_future.IsReady());

  // Advance virtual time by 5 seconds (observation delay timeout is 10s).
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(invoke_future.IsReady());

  // Now, simulate a cross-document navigation. This should restart the
  // observation delay.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  fake_web_state->OnNavigationStarted(&context);

  // Fast forward by 6 seconds.
  // The first timer would have fired at 10s, but it was restarted, so it
  // shouldn't fire yet.
  task_environment_.FastForwardBy(base::Seconds(6));
  EXPECT_FALSE(invoke_future.IsReady());

  // Fast forward by another 4 seconds (total 10s after the navigation).
  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(invoke_future.Get().IsOk());
}

// Tests that deleting the ToolController synchronously inside its completion
// callback does not cause a Use-After-Free (UAF) crash when executing a tool.
TEST_F(ToolControllerTest, DeletingControllerInCompletionCallbackDoesNotUAF) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  bool tool_destroyed = false;
  bool callback_completed_safely = false;

  tool_factory_ = std::make_unique<SyncActorToolFactory>(
      profile_.get(), &tool_destroyed, &callback_completed_safely);
  controller_ = std::make_unique<ToolController>(this);

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  base::test::TestFuture<ToolExecutionResult> validation_future;
  controller_->CreateToolAndValidate(*request, validation_future.GetCallback());
  EXPECT_TRUE(validation_future.Get().IsOk());

  base::RunLoop run_loop;

  // We invoke the tool and inside the completion callback, we synchronously
  // delete the ToolController instance.
  controller_->Invoke(base::BindOnce(
      [](std::unique_ptr<ToolController>* controller, base::RunLoop* loop,
         ToolExecutionResult result) {
        controller->reset();
        loop->Quit();
      },
      &controller_, &run_loop));

  run_loop.Run();

  EXPECT_EQ(controller_, nullptr);
  EXPECT_TRUE(tool_destroyed);
  EXPECT_TRUE(callback_completed_safely);
}

// Tests that deleting the ToolController synchronously inside its validation
// completion callback does not cause a Use-After-Free (UAF) crash.
TEST_F(ToolControllerTest, DeletingControllerInValidationCallbackDoesNotUAF) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  controller_ = std::make_unique<ToolController>(this);

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  base::RunLoop run_loop;

  // We validate the tool and inside the validation callback, we synchronously
  // delete the ToolController instance.
  controller_->CreateToolAndValidate(
      *request, base::BindOnce(
                    [](std::unique_ptr<ToolController>* controller,
                       base::RunLoop* loop, ToolExecutionResult result) {
                      controller->reset();
                      loop->Quit();
                    },
                    &controller_, &run_loop));

  run_loop.Run();

  EXPECT_EQ(controller_, nullptr);
}

}  // namespace
}  // namespace actor
