// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

class MockActorTool : public ActorTool {
 public:
  MOCK_METHOD(void, Execute, (ToolExecutionCallback callback), (override));

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return nullptr;
  }

  optimization_guide::proto::Action::ActionCase GetActionCase() const override {
    return optimization_guide::proto::Action::ACTION_NOT_SET;
  }
};

class MockActorToolFactory : public ActorToolFactory {
 public:
  MOCK_METHOD((base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult>),
              CreateTool,
              (const optimization_guide::proto::Action& action,
               ProfileIOS* profile),
              (override));
};

}  // namespace

class ActorServiceJournalTest : public PlatformTest {
 public:
  ActorServiceJournalTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ActorServiceJournalTest, ToolCreationFails) {
  auto mock_factory = std::make_unique<MockActorToolFactory>();
  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(base::unexpected(
          ToolExecutionResult(InternalToolErrorCode::kUnsupportedAction))));
  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(std::move(action));
  auto tools_result = service.CreateActorTools(actions, ActorTaskId());
  EXPECT_FALSE(tools_result.has_value());

  AggregatedJournal* journal = service.GetJournal();
  ASSERT_NE(nullptr, journal);
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(2u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);
  EXPECT_EQ("Attempting to create tool: NavigateTool", logs[0].event);
  EXPECT_EQ(JournalEntryType::kInstant, logs[1].type);
  EXPECT_EQ("Failed to create tool: NavigateTool", logs[1].event);

  ASSERT_EQ(1u, logs[1].details.size());
  EXPECT_EQ("error", logs[1].details[0].key);
  EXPECT_EQ(GetToolExecutionResultMessage(
                ToolExecutionResult(InternalToolErrorCode::kUnsupportedAction)),
            logs[1].details[0].value);
}

TEST_F(ActorServiceJournalTest, ToolExecutionFails) {
  auto mock_factory = std::make_unique<MockActorToolFactory>();
  auto mock_tool = std::make_unique<MockActorTool>();
  MockActorTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ToolExecutionCallback callback) {
        std::move(callback).Run(
            ToolExecutionResult(InternalToolErrorCode::kNavigationInvalidURL));
      });

  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(std::move(action));

  ActorTaskId task_id = service.CreateTask("Test Task", false);
  auto tools_result = service.CreateActorTools(actions, task_id);
  ASSERT_TRUE(tools_result.has_value());
  service.PerformActions(task_id, std::move(tools_result.value()), "Testing",
                         base::DoNothing());

  AggregatedJournal* journal = service.GetJournal();
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(10u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);  // Attempting.
  EXPECT_EQ(JournalEntryType::kInstant, logs[1].type);  // ActorTask::SetState.
  EXPECT_EQ(JournalEntryType::kInstant, logs[2].type);  // ActorEngine::Act.
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[3].type);  // StateChange (PreExecutionChecks).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[4].type);  // StateChange (ToolVerify).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[5].type);  // StateChange (UiPreInvoke).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[6].type);  // StateChange (ToolInvoke).
  EXPECT_EQ(JournalEntryType::kBegin, logs[7].type);    // Execute Begin.
  EXPECT_EQ(JournalEntryType::kEnd, logs[8].type);      // Execute End.
  EXPECT_EQ(JournalEntryType::kInstant, logs[9].type);  // StateChange (Failed).

  // Verify error detail in End log
  ASSERT_EQ(1u, logs[8].details.size());
  EXPECT_EQ("error", logs[8].details[0].key);
  EXPECT_EQ(GetToolExecutionResultMessage(ToolExecutionResult(
                InternalToolErrorCode::kNavigationInvalidURL)),
            logs[8].details[0].value);
}

TEST_F(ActorServiceJournalTest, ToolExecutionSucceeds) {
  auto mock_factory = std::make_unique<MockActorToolFactory>();
  auto mock_tool = std::make_unique<MockActorTool>();
  MockActorTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ToolExecutionCallback callback) {
        std::move(callback).Run(ToolExecutionResult::Ok());
      });

  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(std::move(action));

  ActorTaskId task_id = service.CreateTask("Test Task", false);
  auto tools_result = service.CreateActorTools(actions, task_id);
  ASSERT_TRUE(tools_result.has_value());
  service.PerformActions(task_id, std::move(tools_result.value()), "Testing",
                         base::DoNothing());

  AggregatedJournal* journal = service.GetJournal();
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(11u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);  // Attempting.
  EXPECT_EQ(JournalEntryType::kInstant, logs[1].type);  // ActorTask::SetState.
  EXPECT_EQ(JournalEntryType::kInstant, logs[2].type);  // ActorEngine::Act.
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[3].type);  // StateChange (PreExecutionChecks).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[4].type);  // StateChange (ToolVerify).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[5].type);  // StateChange (UiPreInvoke).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[6].type);  // StateChange (ToolInvoke).
  EXPECT_EQ(JournalEntryType::kBegin, logs[7].type);  // Execute Begin.
  EXPECT_EQ("Execute Tool 0", logs[7].event);
  EXPECT_EQ(JournalEntryType::kEnd, logs[8].type);  // Execute End.
  EXPECT_EQ("Execute Tool 0", logs[8].event);
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[9].type);  // StateChange (UiPostInvoke).
  EXPECT_EQ(JournalEntryType::kInstant,
            logs[10].type);  // StateChange (Completed).

  // Verify no error detail in End log on success
  EXPECT_EQ(0u, logs[8].details.size());
}

}  // namespace actor
