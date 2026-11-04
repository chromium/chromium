// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"

#import <vector>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace actor {

namespace {

constexpr int kTabId = 42;

struct ToolRequestTestCase {
  optimization_guide::proto::Action action;
  ToolType expected_type;
  bool expected_requires_url_check;
};

std::vector<ToolRequestTestCase> GetTestCases() {
  std::vector<ToolRequestTestCase> test_cases;
  {
    optimization_guide::proto::Action action;
    action.mutable_click()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kClick, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_type()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kType, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_scroll()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kScroll, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_select()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kSelect, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_drag_and_release()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kDragAndRelease, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_navigate()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kNavigate, false});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_back()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kBack, false});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_forward()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kForward, false});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_wait()->set_observe_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kWait, false});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_scroll_to()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kScrollTo, true});
  }
  {
    optimization_guide::proto::Action action;
    action.mutable_activate_tab()->set_tab_id(kTabId);
    test_cases.push_back({action, ToolType::kActivateTab, true});
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
  EXPECT_EQ(request.RequiresUrlCheckInCurrentTab(),
            test_case.expected_requires_url_check);
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
  EXPECT_FALSE(request.RequiresUrlCheckInCurrentTab());
}

// Tests the behavior of the create tab action.
TEST_F(ActorToolRequestSimpleTest, CreateTabAction) {
  optimization_guide::proto::Action action;
  action.mutable_create_tab();
  ActorToolRequest request(action);

  EXPECT_EQ(request.GetToolType(), ToolType::kCreateTab);
  EXPECT_FALSE(request.GetTargetWebStateId().valid());
  EXPECT_FALSE(request.RequiresUrlCheckInCurrentTab());
}

// Verifies the behavior when an action that requires a tab_id doesn't have it
// set.
TEST_F(ActorToolRequestSimpleTest, MissingTabId) {
  optimization_guide::proto::Action action;
  action.mutable_click();
  ActorToolRequest request(action);

  EXPECT_EQ(request.GetToolType(), ToolType::kClick);
  EXPECT_FALSE(request.GetTargetWebStateId().valid());
  EXPECT_FALSE(request.RequiresUrlCheckInCurrentTab());
}

// Tests that a valid NavigateAction URL grants its origin.
TEST_F(ActorToolRequestSimpleTest, AssociatedOriginGrant_ValidNavigateUrl) {
  optimization_guide::proto::Action navigate_action;
  navigate_action.mutable_navigate()->set_url("https://example.com/path?q=1");
  ActorToolRequest navigate_request(navigate_action);
  std::optional<url::Origin> grant = navigate_request.AssociatedOriginGrant();
  ASSERT_TRUE(grant.has_value());
  EXPECT_EQ(*grant, url::Origin::Create(GURL("https://example.com")));
}

// Tests that non-Navigate actions return nullopt.
TEST_F(ActorToolRequestSimpleTest,
       AssociatedOriginGrant_ReturnsNulloptForNonNavigateAction) {
  optimization_guide::proto::Action click_action;
  click_action.mutable_click()->set_tab_id(kTabId);

  EXPECT_FALSE(
      ActorToolRequest(click_action).AssociatedOriginGrant().has_value());
}

// Tests that a NavigateAction with an invalid URL string returns nullopt.
TEST_F(ActorToolRequestSimpleTest,
       AssociatedOriginGrant_ReturnsNulloptForInvalidUrl) {
  optimization_guide::proto::Action invalid_url_action;
  invalid_url_action.mutable_navigate()->set_url("not a url");

  EXPECT_FALSE(
      ActorToolRequest(invalid_url_action).AssociatedOriginGrant().has_value());
}

// Tests that a NavigateAction resulting in an opaque origin (like about:blank)
// returns nullopt.
TEST_F(ActorToolRequestSimpleTest,
       AssociatedOriginGrant_ReturnsNulloptForOpaqueOrigin) {
  optimization_guide::proto::Action opaque_url_action;
  opaque_url_action.mutable_navigate()->set_url("about:blank");

  EXPECT_FALSE(
      ActorToolRequest(opaque_url_action).AssociatedOriginGrant().has_value());
}
}  // namespace

}  // namespace actor
