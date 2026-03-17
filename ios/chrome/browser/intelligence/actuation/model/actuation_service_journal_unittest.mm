// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_service.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_service_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"
#import "ios/chrome/browser/intelligence/actuation/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationCallback = ActuationTool::ActuationCallback;

namespace {

class MockActuationTool : public ActuationTool {
 public:
  MOCK_METHOD(void, Execute, (ActuationCallback callback), (override));
};

class MockActuationToolFactory : public ActuationToolFactory {
 public:
  MOCK_METHOD((base::expected<std::unique_ptr<ActuationTool>, ActuationError>),
              CreateTool,
              (const optimization_guide::proto::Action& action,
               ProfileIOS* profile),
              (override));
};

}  // namespace

class ActuationServiceJournalTest : public PlatformTest {
 public:
  ActuationServiceJournalTest() {
    scoped_feature_list_.InitAndEnableFeature(kActuationTools);
    ActuationServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ActuationServiceJournalTest, ToolCreationFails) {
  auto mock_factory = std::make_unique<MockActuationToolFactory>();
  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(base::unexpected(
          ActuationError{ActuationErrorCode::kUnsupportedAction})));
  ActuationService service(profile_.get(), std::move(mock_factory));

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

TEST_F(ActuationServiceJournalTest, ToolExecutionFails) {
  auto mock_factory = std::make_unique<MockActuationToolFactory>();
  auto mock_tool = std::make_unique<MockActuationTool>();
  MockActuationTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ActuationCallback callback) {
        std::move(callback).Run(base::unexpected(
            ActuationError{ActuationErrorCode::kNavigationInvalidURL}));
      });

  ActuationService service(profile_.get(), std::move(mock_factory));

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
                static_cast<int>(ActuationErrorCode::kNavigationInvalidURL)),
            logs[2].details[0].value);
}

TEST_F(ActuationServiceJournalTest, ToolExecutionSucceeds) {
  auto mock_factory = std::make_unique<MockActuationToolFactory>();
  auto mock_tool = std::make_unique<MockActuationTool>();
  MockActuationTool* tool_ptr = mock_tool.get();

  EXPECT_CALL(*mock_factory, CreateTool(testing::_, testing::_))
      .WillOnce(testing::Return(std::move(mock_tool)));

  EXPECT_CALL(*tool_ptr, Execute(testing::_))
      .WillOnce([](ActuationCallback callback) {
        std::move(callback).Run(base::ok());
      });

  ActuationService service(profile_.get(), std::move(mock_factory));

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
