// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/page_stability_java_script_feature.h"

#import "base/test/test_future.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

class PageStabilityJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  PageStabilityJavaScriptFeatureTest() : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  PageStabilityJavaScriptFeature* feature() {
    return PageStabilityJavaScriptFeature::GetInstance();
  }

  void MockWaitForLcpJsFunction(const std::string& mock_return_value) {
    MockJsFunction(feature(), "page_stability", "waitForLcp",
                   mock_return_value);
  }
};

// Test that WaitForLcp returns kFrameWentAway when target_frame is null.
TEST_F(PageStabilityJavaScriptFeatureTest, WaitForLcp_InvalidatedWebFrame) {
  base::test::TestFuture<ToolExecutionResult> future;
  feature()->WaitForLcp(/*target_frame=*/nullptr, /*timeout=*/base::Seconds(1),
                        future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Test that WaitForLcp succeeds when JS returns lcpReceived = true.
TEST_F(PageStabilityJavaScriptFeatureTest, WaitForLcp_Success) {
  MockWaitForLcpJsFunction(/*mock_return_value=*/"{lcpReceived: true}");
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->WaitForLcp(GetMainFrame(feature()), /*timeout=*/base::Seconds(1),
                        future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

// Test that WaitForLcp succeeds even when JS returns lcpReceived = false.
TEST_F(PageStabilityJavaScriptFeatureTest, WaitForLcp_LcpNotReceived) {
  MockWaitForLcpJsFunction(/*mock_return_value=*/"{lcpReceived: false}");
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->WaitForLcp(GetMainFrame(feature()), /*timeout=*/base::Seconds(1),
                        future.GetCallback());

  auto result = future.Get();
  EXPECT_TRUE(result.IsOk());
}

// Test that WaitForLcp returns kArgumentsInvalid when JS returns a dict missing
// lcpReceived field.
TEST_F(PageStabilityJavaScriptFeatureTest, WaitForLcp_MissingLcpReceivedField) {
  MockWaitForLcpJsFunction(/*mock_return_value=*/"{}");
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->WaitForLcp(GetMainFrame(feature()), /*timeout=*/base::Seconds(1),
                        future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that WaitForLcp returns kArgumentsInvalid when JS returns a
// non-dictionary result.
TEST_F(PageStabilityJavaScriptFeatureTest, WaitForLcp_InvalidResult) {
  MockWaitForLcpJsFunction(/*mock_return_value=*/"'unexpected string'");
  base::test::TestFuture<ToolExecutionResult> future;

  feature()->WaitForLcp(GetMainFrame(feature()), /*timeout=*/base::Seconds(1),
                        future.GetCallback());

  auto result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that CancelWaitForLcp handles null frame safely without crashing.
TEST_F(PageStabilityJavaScriptFeatureTest, CancelWaitForLcp_NullFrame) {
  feature()->CancelWaitForLcp(/*target_frame=*/nullptr);
}

}  // namespace actor
