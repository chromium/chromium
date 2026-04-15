// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_error.h"
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
};

class MockActorToolFactory : public ActorToolFactory {
 public:
  MOCK_METHOD((base::expected<std::unique_ptr<ActorTool>, ActorToolError>),
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
          ActorToolError{ActorToolErrorCode::kUnsupportedAction})));
  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  service.ExecuteAction(action, base::DoNothing());

  AggregatedJournal* journal = service.GetJournal();
  ASSERT_NE(nullptr, journal);
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(2u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);
  EXPECT_EQ("Attempting to create tool: NavigateTool", logs[0].event);
  EXPECT_EQ(JournalEntryType::kInstant, logs[1].type);
  EXPECT_EQ("Failed to create tool: NavigateTool", logs[1].event);
}

TEST_F(ActorServiceJournalTest, ToolExecutionFails) {
  auto mock_factory = std::make_unique<MockActorToolFactory>();
  auto mock_tool = std::make_unique<MockActorTool>();
  MockActorTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ToolExecutionCallback callback) {
        std::move(callback).Run(base::unexpected(
            ActorToolError{ActorToolErrorCode::kNavigationInvalidURL}));
      });

  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");

  service.ExecuteAction(action, base::DoNothing());

  AggregatedJournal* journal = service.GetJournal();
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(3u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);  // Attempting
  EXPECT_EQ(JournalEntryType::kBegin, logs[1].type);    // Execute Begin
  EXPECT_EQ(JournalEntryType::kEnd, logs[2].type);      // Execute End

  // Verify error detail in End log
  ASSERT_EQ(1u, logs[2].details.size());
  EXPECT_EQ("error", logs[2].details[0].key);
  EXPECT_EQ(base::NumberToString(
                static_cast<int>(ActorToolErrorCode::kNavigationInvalidURL)),
            logs[2].details[0].value);
}

TEST_F(ActorServiceJournalTest, ToolExecutionSucceeds) {
  auto mock_factory = std::make_unique<MockActorToolFactory>();
  auto mock_tool = std::make_unique<MockActorTool>();
  MockActorTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ToolExecutionCallback callback) {
        std::move(callback).Run(base::ok());
      });

  ActorService service(profile_.get(), std::move(mock_factory));

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");

  service.ExecuteAction(action, base::DoNothing());

  AggregatedJournal* journal = service.GetJournal();
  std::vector<JournalEntry> logs = journal->GetLogs();

  ASSERT_EQ(3u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);  // Attempting
  EXPECT_EQ(JournalEntryType::kBegin, logs[1].type);    // Execute Begin
  EXPECT_EQ("Execute Tool: NavigateTool", logs[1].event);
  EXPECT_EQ(JournalEntryType::kEnd, logs[2].type);  // Execute End
  EXPECT_EQ("Execute Tool: NavigateTool", logs[2].event);

  // Verify no error detail in End log on success
  EXPECT_EQ(0u, logs[2].details.size());
}

}  // namespace actor
