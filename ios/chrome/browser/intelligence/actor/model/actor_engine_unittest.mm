// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/run_loop.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/safety_list_manager.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/origin_gating/core/origin_gating_configuration.h"
#import "components/origin_gating/core/origin_gating_registration.h"
#import "components/origin_gating/core/origin_gating_service.h"
#import "components/origin_gating/core/types.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_controller.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

struct DelegateCall {
  ToolType tool_type;
  web::WebStateID web_state_id;
};

// A mock ActorEngine::ExecutionUpdatesDelegate for testing.
class MockActorEngineExecutionUpdatesDelegate
    : public ActorEngine::ExecutionUpdatesDelegate {
 public:
  MockActorEngineExecutionUpdatesDelegate() = default;
  ~MockActorEngineExecutionUpdatesDelegate() override = default;

  void OnWillExecuteTool(ToolType tool_type,
                         web::WebStateID web_state_id) override {
    calls_.push_back({tool_type, web_state_id});
    on_will_execute_called_ = true;
  }

  std::vector<DelegateCall> calls_;
  bool on_will_execute_called_ = false;
};

}  // namespace

// Test fixture for ActorEngine.
class ActorEngineTest : public PlatformTest {
 protected:
  ActorEngineTest() { scoped_feature_list_.InitAndEnableFeature(kActorTools); }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    journal_ = std::make_unique<AggregatedJournal>();
    tool_factory_ = std::make_unique<ActorToolFactory>(profile_.get());
    task_ = std::make_unique<ActorTask>(
        ActorTaskId(1), "Test Task",
        /*allow_incognito_web_states=*/false, journal_.get(),
        tool_factory_.get(), BrowserListFactory::GetForProfile(profile_.get()));
    engine_ = std::make_unique<ActorEngine>(&execution_updates_delegate_,
                                            task_.get());
  }

  void TearDown() override {
    engine_.reset();
    task_.reset();
    tool_factory_.reset();
    journal_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  void SetNextActionIndex(size_t index) { engine_->next_action_index_ = index; }

  size_t InProgressActionIndex() const {
    return engine_->InProgressActionIndex();
  }

  void PushActionResult(ActionResult result) {
    engine_->action_results_.push_back(std::move(result));
  }

  const std::vector<ActionResult>& GetActionResults() const {
    return engine_->action_results_;
  }

  ActorEngine::State GetState() const { return engine_->state_; }

  ToolController* GetToolController() const {
    return engine_->tool_controller_.get();
  }

  void CompleteActions(ActionResult&& result) {
    engine_->CompleteActions(std::move(result));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<AggregatedJournal> journal_;
  std::unique_ptr<ActorToolFactory> tool_factory_;
  std::unique_ptr<ActorTask> task_;
  MockActorEngineExecutionUpdatesDelegate execution_updates_delegate_;
  std::unique_ptr<ActorEngine> engine_;
};

// Tests that ToolDelegate methods on ActorEngine properly forward to the task
// and own handler objects.
TEST_F(ActorEngineTest, ToolDelegateForwardingAndOwnership) {
  ToolDelegate* tool_delegate = engine_.get();
  EXPECT_EQ(tool_delegate->GetTaskId(), ActorTaskId(1));
  EXPECT_EQ(&tool_delegate->GetJournal(), journal_.get());
  EXPECT_EQ(&tool_delegate->GetToolFactory(), tool_factory_.get());
  EXPECT_NE(tool_delegate->GetActorTaskFormFillingHandler(), nullptr);
}

// Tests that ToolDelegate InterruptFromTool and UninterruptFromTool change
// task state properly.
TEST_F(ActorEngineTest, ToolDelegateInterruptAndUninterrupt) {
  ToolDelegate* tool_delegate = engine_.get();
  base::test::TestFuture<std::vector<ActionResult>> future;
  task_->Act({}, "test update", future.GetCallback());
  EXPECT_TRUE(future.Wait());
  ASSERT_EQ(task_->GetState(), ActorTaskState::kReflecting);

  tool_delegate->InterruptFromTool();
  EXPECT_EQ(task_->GetState(), ActorTaskState::kWaitingOnUser);

  tool_delegate->UninterruptFromTool();
  EXPECT_EQ(task_->GetState(), ActorTaskState::kActing);
}

// Tests that a single action executing successfully completes the engine
// sequence with a success result.
TEST_F(ActorEngineTest, ActSuccess) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(MakeSuccessfulActorToolRequest());

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 1U);
  EXPECT_TRUE(results[0].tool_result.IsOk());
}

// Tests that a single action failing aborts the engine sequence and returns a
// failure result.
TEST_F(ActorEngineTest, ActFailure) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(MakeFailingActorToolRequest());

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 1U);
  EXPECT_FALSE(results[0].tool_result.IsOk());
}

// Tests that a sequence where the first action succeeds and the second fails
// returns both results, with the second one indicating failure.
TEST_F(ActorEngineTest, ActSequenceSuccessFailure) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(MakeSuccessfulActorToolRequest());
  actions.push_back(MakeFailingActorToolRequest());

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_FALSE(results[1].tool_result.IsOk());
}

// Tests that an empty sequence of actions completes immediately with success
// and empty results.
TEST_F(ActorEngineTest, ActEmptySequence) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(results.empty());
}

// Tests that multiple actions all executing successfully return success results
// for all actions.
TEST_F(ActorEngineTest, ActMultipleSuccess) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(MakeSuccessfulActorToolRequest());
  actions.push_back(MakeSuccessfulActorToolRequest());

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_TRUE(results[1].tool_result.IsOk());
}

// Tests the helper method that maps the 1-based `next_action_index_` to the
// 0-based current action index.
TEST_F(ActorEngineTest, InProgressActionIndex) {
  SetNextActionIndex(1);
  EXPECT_EQ(InProgressActionIndex(), 0U);

  SetNextActionIndex(2);
  EXPECT_EQ(InProgressActionIndex(), 1U);
}

// Tests the specific codepath in `CompleteActions` where a failure result
// overwrites a previously recorded success for the same action (e.g., if a
// post-invoke step fails).
TEST_F(ActorEngineTest, CompleteActionsOverwrite) {
  PushActionResult(ActionResult(ToolExecutionResult::Ok()));
  SetNextActionIndex(1);

  CompleteActions(ActionResult(
      ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid)));

  EXPECT_EQ(GetActionResults().size(), 1U);
  EXPECT_FALSE(GetActionResults()[0].tool_result.IsOk());
  EXPECT_EQ(GetState(), ActorEngine::State::kFailed);
}

// Tests that the delegate's OnWillExecuteTool callback is fired
// just before tool execution with correct, unique parameters for every tool in
// the sequence.
TEST_F(ActorEngineTest, OnWillExecuteToolCalled) {
  TestProfileIOS* profile = profile_.get();
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  auto browser = std::make_unique<TestBrowser>(profile);
  browser_list->AddBrowser(browser.get());

  auto web_state1 = std::make_unique<web::FakeWebState>();
  web_state1->SetBrowserState(profile);
  web::WebStateID id1 = web_state1->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state1),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web_state2->SetBrowserState(profile);
  web::WebStateID id2 = web_state2->GetUniqueIdentifier();
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;

  optimization_guide::proto::Action action1;
  auto* wait1 = action1.mutable_wait();
  wait1->set_observe_tab_id(id1.identifier());
  wait1->set_wait_time_ms(0);
  actions.push_back(std::make_unique<ActorToolRequest>(action1));

  optimization_guide::proto::Action action2;
  auto* wait2 = action2.mutable_wait();
  wait2->set_observe_tab_id(id2.identifier());
  wait2->set_wait_time_ms(0);
  actions.push_back(std::make_unique<ActorToolRequest>(action2));

  base::RunLoop run_loop;
  engine_->Act(
      std::move(actions),
      base::BindOnce([](base::RunLoop* loop,
                        std::vector<ActionResult> res) { loop->Quit(); },
                     &run_loop));

  run_loop.Run();

  EXPECT_TRUE(execution_updates_delegate_.on_will_execute_called_);
  ASSERT_GE(execution_updates_delegate_.calls_.size(), 2U);

  EXPECT_EQ(execution_updates_delegate_.calls_[0].tool_type, ToolType::kWait);
  EXPECT_EQ(execution_updates_delegate_.calls_[0].web_state_id, id1);

  EXPECT_EQ(execution_updates_delegate_.calls_[1].tool_type, ToolType::kWait);
  EXPECT_EQ(execution_updates_delegate_.calls_[1].web_state_id, id2);
}

// Tests that executing a sequence containing a null tool completes
// with a failure result code (kToolUnknown) instead of crashing.
TEST_F(ActorEngineTest, ActWithNullTool) {
  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(nullptr);

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_->Act(std::move(actions),
               base::BindOnce(
                   [](bool* called, std::vector<ActionResult>* res_out,
                      base::RunLoop* loop, std::vector<ActionResult> res) {
                     *called = true;
                     res_out->swap(res);
                     loop->Quit();
                   },
                   &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 1U);
  EXPECT_FALSE(results[0].tool_result.IsOk());
  EXPECT_EQ(results[0].tool_result.code(),
            mojom::ActionResultCode::kToolUnknown);
}

// Test that FailCurrentTool is a safe no-op when the engine is not in the
// kToolInvoke state.
TEST_F(ActorEngineTest, FailCurrentToolWhenNotInToolInvoke) {
  EXPECT_EQ(GetState(), ActorEngine::State::kInit);
  engine_->FailCurrentTool(
      mojom::ActionResultCode::kTriggeredNavigationBlocked);
  EXPECT_EQ(GetState(), ActorEngine::State::kInit);
}

// Test that calling FailCurrentTool while a tool is in-flight aborts the tool,
// transitions the engine to kFailed, and reports the error code.
TEST_F(ActorEngineTest, FailCurrentToolMidExecution) {
  // Create a WaitAction with a 10-second duration so it stays in-flight.
  optimization_guide::proto::Action action;
  auto* wait = action.mutable_wait();
  wait->set_wait_time_ms(10000);

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(std::make_unique<ActorToolRequest>(action));

  base::test::TestFuture<std::vector<ActionResult>> future;
  engine_->Act(std::move(actions), future.GetCallback());

  // Wait until tool validation completes and the tool starts executing.
  ASSERT_TRUE(base::test::RunUntil([this]() {
    ToolController* controller = GetToolController();
    return controller &&
           controller->state() == ToolController::State::kInvoking;
  }));
  ASSERT_EQ(GetState(), ActorEngine::State::kToolInvoke);

  // Preemptively fail the in-flight tool.
  engine_->FailCurrentTool(
      mojom::ActionResultCode::kTriggeredNavigationBlocked);

  std::vector<ActionResult> results = future.Take();
  ASSERT_EQ(results.size(), 1U);
  EXPECT_FALSE(results[0].tool_result.IsOk());
  EXPECT_EQ(results[0].tool_result.code(),
            mojom::ActionResultCode::kTriggeredNavigationBlocked);
  EXPECT_EQ(GetState(), ActorEngine::State::kFailed);
}

class ActorEngineOriginGatingTest : public ActorEngineTest {
 public:
  void SetUp() override {
    ActorEngineTest::SetUp();
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({kActorTools, kActorOriginGating},
                                          {});
    checker_ = engine_->GetOriginGatingChecker();
    ASSERT_NE(checker_, nullptr);
  }

  void TearDown() override {
    // Clear raw_ptr before ActorEngineTest::TearDown() destroys engine_.
    checker_ = nullptr;
    // Reset the singleton to prevent safety list rules from leaking into
    // subsequent tests.
    actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                      "{}");
    ActorEngineTest::TearDown();
  }

 protected:
  raw_ptr<origin_gating::OriginGatingChecker> checker_;
};

// Verify the checker is created and available.
TEST_F(ActorEngineOriginGatingTest, CheckerInitialized) {
  // Verify non-null in SetUp().
  EXPECT_NE(checker_, nullptr);
}

// Verify that each engine receives its own isolated OriginGatingChecker
// instance and registration ID.
TEST_F(ActorEngineOriginGatingTest, EnginesHaveIsolatedCheckers) {
  ActorEngine engine_2(&execution_updates_delegate_, task_.get());
  EXPECT_NE(engine_->GetOriginGatingCheckerId(),
            engine_2.GetOriginGatingCheckerId());
  EXPECT_NE(engine_->GetOriginGatingChecker(),
            engine_2.GetOriginGatingChecker());
}

// Verify that destroying an engine unregisters its checker from
// OriginGatingService.
TEST_F(ActorEngineOriginGatingTest,
       DestroyEngine_UnregistersOriginGatingChecker) {
  origin_gating::CheckerId checker_id = engine_->GetOriginGatingCheckerId();
  origin_gating::OriginGatingService* gating_service =
      origin_gating::OriginGatingServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(gating_service, nullptr);
  EXPECT_NE(gating_service->GetChecker(checker_id), nullptr);

  checker_ = nullptr;
  engine_.reset();
  EXPECT_EQ(gating_service->GetChecker(checker_id), nullptr);
}

// Verify that a blocked URL in the safety list triggers kBlocked.
TEST_F(ActorEngineOriginGatingTest, BlocksListedUrl) {
  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://safe.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  base::test::TestFuture<std::unique_ptr<origin_gating::GatingDecisionContext>,
                         origin_gating::GatingDecision>
      future;
  checker_->ComputeGatingDecision(
      std::make_unique<origin_gating::GatingDecisionContext>(),
      origin_gating::GateableEvent::kNavigationResponse,
      GURL("https://safe.com"), GURL("https://malicious.com"),
      future.GetCallback());

  const origin_gating::GatingDecision& decision = future.Get<1>();
  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, ActorCustomPredicate::kSafetyList);
}

// Verify that an unlisted URL on kNavigationRequest fails open (kAllowed).
TEST_F(ActorEngineOriginGatingTest, NavigationRequestFailsOpenForUnlistedUrl) {
  base::test::TestFuture<std::unique_ptr<origin_gating::GatingDecisionContext>,
                         origin_gating::GatingDecision>
      future;
  checker_->ComputeGatingDecision(
      std::make_unique<origin_gating::GatingDecisionContext>(),
      origin_gating::GateableEvent::kNavigationRequest,
      GURL("https://random-source.com"), GURL("https://random-dest.com"),
      future.GetCallback());

  const origin_gating::GatingDecision& decision = future.Get<1>();
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, origin_gating::DecisionSource::kNoVerdict);
}

// Verify that an allowlisted URL in the safety list is allowed.
TEST_F(ActorEngineOriginGatingTest, AllowsListedUrl) {
  const std::string mock_rules_json = R"({
    "navigation_allowed":[
      {"from": "https://safe.com", "to": "https://trusted.com"}
    ]
  })";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  base::test::TestFuture<std::unique_ptr<origin_gating::GatingDecisionContext>,
                         origin_gating::GatingDecision>
      future;
  checker_->ComputeGatingDecision(
      std::make_unique<origin_gating::GatingDecisionContext>(),
      origin_gating::GateableEvent::kNavigationResponse,
      GURL("https://safe.com"), GURL("https://trusted.com"),
      future.GetCallback());

  const origin_gating::GatingDecision& decision = future.Get<1>();
  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, ActorCustomPredicate::kSafetyList);
}

// Verify that kPageAction events are checked against the safety list.
TEST_F(ActorEngineOriginGatingTest, BlocksActionOnBlockedUrl) {
  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://safe.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  base::test::TestFuture<std::unique_ptr<origin_gating::GatingDecisionContext>,
                         origin_gating::GatingDecision>
      future;
  checker_->ComputeGatingDecision(
      std::make_unique<origin_gating::GatingDecisionContext>(),
      origin_gating::GateableEvent::kPageAction, GURL("https://safe.com"),
      GURL("https://malicious.com"), future.GetCallback());

  const origin_gating::GatingDecision& decision = future.Get<1>();
  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, ActorCustomPredicate::kSafetyList);
}

// Verify that an empty source URL falls back to destination.
TEST_F(ActorEngineOriginGatingTest, HandlesEmptySourceUrl) {
  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://malicious.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  base::test::TestFuture<std::unique_ptr<origin_gating::GatingDecisionContext>,
                         origin_gating::GatingDecision>
      future;
  checker_->ComputeGatingDecision(
      std::make_unique<origin_gating::GatingDecisionContext>(),
      origin_gating::GateableEvent::kNavigationResponse,
      /*source=*/GURL(), GURL("https://malicious.com"), future.GetCallback());

  const origin_gating::GatingDecision& decision = future.Get<1>();
  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.attribution, ActorCustomPredicate::kSafetyList);
}

// Test that a NavigateAction executed through ActorEngine is blocked when
// disallowed by the OriginGatingChecker safety list.
TEST_F(ActorEngineOriginGatingTest, Act_NavigationBlockedByOriginGating) {
  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://safe.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(test_browser.get());
  UrlLoadingBrowserAgent::CreateForBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
  fake_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  fake_web_state->SetCurrentURL(GURL("https://safe.com"));
  int tab_id = fake_web_state->GetUniqueIdentifier().identifier();
  test_browser->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://malicious.com");
  action.mutable_navigate()->set_tab_id(tab_id);

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(std::make_unique<ActorToolRequest>(action));

  base::test::TestFuture<std::vector<ActionResult>> future;
  engine_->Act(std::move(actions), future.GetCallback());
  const std::vector<ActionResult>& results = future.Get();

  ASSERT_EQ(1u, results.size());
  EXPECT_FALSE(results[0].tool_result.IsOk());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTriggeredNavigationBlocked,
            results[0].tool_result.code());
}

// Test that a NavigateAction executed through ActorEngine succeeds when
// allowed by the OriginGatingChecker.
TEST_F(ActorEngineOriginGatingTest, Act_NavigationAllowedByOriginGating) {
  const std::string mock_rules_json = R"json({
    "navigation_allowed": [
      {"from": "https://safe.com", "to": "https://trusted.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(test_browser.get());
  UrlLoadingBrowserAgent::CreateForBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* fake_navigation_manager_ptr =
      fake_navigation_manager.get();
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
  fake_web_state->SetCurrentURL(GURL("https://safe.com"));
  int tab_id = fake_web_state->GetUniqueIdentifier().identifier();
  test_browser->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://trusted.com");
  action.mutable_navigate()->set_tab_id(tab_id);

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(std::make_unique<ActorToolRequest>(action));

  base::test::TestFuture<std::vector<ActionResult>> future;
  engine_->Act(std::move(actions), future.GetCallback());
  const std::vector<ActionResult>& results = future.Get();

  ASSERT_EQ(1u, results.size());
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_TRUE(fake_navigation_manager_ptr->LoadURLWithParamsWasCalled());
  EXPECT_EQ(GURL("https://trusted.com"),
            fake_navigation_manager_ptr->GetLastLoadURLWithParams()->url);
}

// Test that an implicit navigation (e.g. link click / redirect) on a controlled
// WebState is cancelled by origin gating and does not terminate the ActorTask.
TEST_F(
    ActorEngineOriginGatingTest,
    ImplicitNavigation_BlockedByOriginGating_CancelsNavigationWithoutStoppingTask) {
  // Configure safety list rules.
  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://safe.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  // Set up browser and WebState.
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
  fake_web_state->SetCurrentURL(GURL("https://safe.com"));
  web::FakeWebState* web_state_ptr = fake_web_state.get();

  test_browser->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  // Register the WebState as controlled by the task.
  // This instantiates ActorWebStatePolicyDecider and attaches it to the
  // WebState.
  task_->AddControlledWebState(web_state_ptr);

  // Simulate an implicit main-frame navigation request (e.g. link click).
  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false,
      /*user_tapped_recently*/ false);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;
  web_state_ptr->ShouldAllowRequest(request, request_info,
                                    decision_future.GetCallback());

  // Verify that the navigation was cancelled.
  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());

  // Verify that the task was not stopped; it remains alive so Gemini can handle
  // the block.
  EXPECT_EQ(task_->GetState(), ActorTaskState::kInit);
}

// Test that an implicit navigation (e.g. link click) to an allowed destination
// is permitted by origin gating and does not stop the ActorTask.
TEST_F(ActorEngineOriginGatingTest,
       ImplicitNavigation_AllowedByOriginGating_PermitsNavigation) {
  const std::string mock_rules_json = R"json({
    "navigation_allowed": [
      {"from": "https://safe.com", "to": "https://trusted.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetBrowserState(profile_.get());
  fake_web_state->SetCurrentURL(GURL("https://safe.com"));
  web::FakeWebState* web_state_ptr = fake_web_state.get();

  test_browser->GetWebStateList()->InsertWebState(
      std::move(fake_web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  task_->AddControlledWebState(web_state_ptr);

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://trusted.com"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false,
      /*user_tapped_recently*/ false);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;
  web_state_ptr->ShouldAllowRequest(request, request_info,
                                    decision_future.GetCallback());

  // Verify that the navigation was allowed.
  EXPECT_TRUE(decision_future.Get().ShouldAllowNavigation());

  // Verify that the task was not stopped.
  EXPECT_EQ(task_->GetState(), ActorTaskState::kInit);
}

}  // namespace actor
