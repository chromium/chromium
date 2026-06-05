// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"

#import <vector>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

constexpr int kTabId = 42;

struct ToolRequestTestCase {
  optimization_guide::proto::Action action;
  ToolType expected_type;
};

std::vector<ToolRequestTestCase> GetTestCases() {
  std::vector<ToolRequestTestCase> test_cases;
  {
    optimization_guide::proto::Action action;
    action.mutable_click()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kClick});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_type()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kType});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_scroll()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kScroll});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_select()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kSelect});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_navigate()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kNavigate});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_back()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kBack});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_forward()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kForward});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_wait()->set_observe_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kWait});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_scroll_to()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kScrollTo});
  }
  return test_cases;
}

class ActorToolRequestTest
    : public PlatformTest,
      public ::testing::WithParamInterface<ToolRequestTestCase> {};

TEST_P(ActorToolRequestTest, VerifyToolRequestProperties) {
  const ToolRequestTestCase& test_case = GetParam();
  ActorToolRequest request(test_case.action);

  EXPECT_EQ(request.GetToolType(), test_case.expected_type);
  EXPECT_TRUE(request.GetTargetWebStateId().valid());
  EXPECT_EQ(request.GetTargetWebStateId().identifier(), kTabId);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ActorToolRequestTest,
                         ::testing::ValuesIn(GetTestCases()));

using ActorToolRequestSimpleTest = PlatformTest;

// Verifies the behavior of an unknown action (default case).
TEST_F(ActorToolRequestSimpleTest, UnknownAction) {
  optimization_guide::proto::Action action;
  ActorToolRequest request(action);

  EXPECT_EQ(request.GetToolType(), ToolType::kUnknown);
  EXPECT_FALSE(request.GetTargetWebStateId().valid());
}

// Verifies the behavior when an action that requires a tab_id doesn't have it
// set.
TEST_F(ActorToolRequestSimpleTest, MissingTabId) {
  optimization_guide::proto::Action action;
  action.mutable_click();
  ActorToolRequest request(action);

  EXPECT_EQ(request.GetToolType(), ToolType::kClick);
  EXPECT_FALSE(request.GetTargetWebStateId().valid());
}

}  // namespace

}  // namespace actor
