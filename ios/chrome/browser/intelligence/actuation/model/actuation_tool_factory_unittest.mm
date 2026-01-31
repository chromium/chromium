// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_tool_factory.h"

#import "base/test/task_environment.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ActuationToolFactoryTest : public PlatformTest {
 public:
  using ActuationError = ActuationTool::ActuationError;
  using ActuationErrorCode = ActuationTool::ActuationErrorCode;

  ActuationToolFactoryTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

TEST_F(ActuationToolFactoryTest, CreateTool_DefaultProto) {
  ActuationToolFactory factory;
  optimization_guide::proto::Action action;
  base::expected<std::unique_ptr<ActuationTool>, ActuationError> result =
      factory.CreateTool(action, profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kUnsupportedAction, result.error().code);
}

TEST_F(ActuationToolFactoryTest, CreateTool_NavigateTool_Success) {
  auto web_state = std::make_unique<web::FakeWebState>();
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  action.mutable_navigate()->set_url("https://example.com");
  action.mutable_navigate()->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());

  ActuationToolFactory factory;
  base::expected<std::unique_ptr<ActuationTool>, ActuationError> result =
      factory.CreateTool(action, profile_.get());
  EXPECT_TRUE(result.has_value());
  EXPECT_NE(nullptr, result.value());
}
