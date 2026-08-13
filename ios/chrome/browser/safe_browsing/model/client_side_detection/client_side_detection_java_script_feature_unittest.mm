// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_java_script_feature.h"

#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class ClientSideDetectionJavaScriptFeatureTest : public PlatformTest {
 protected:
  ClientSideDetectionJavaScriptFeatureTest()
      : feature_(ClientSideDetectionJavaScriptFeature::GetInstance()) {}

  web::WebTaskEnvironment task_environment_;
  raw_ptr<ClientSideDetectionJavaScriptFeature> feature_ = nullptr;
};

// Tests that the singleton instance is valid and consistent.
TEST_F(ClientSideDetectionJavaScriptFeatureTest, TestSingleton) {
  ASSERT_TRUE(feature_);
  EXPECT_EQ(feature_, ClientSideDetectionJavaScriptFeature::GetInstance());
}

// Tests that the script message handler name matches expected configuration.
TEST_F(ClientSideDetectionJavaScriptFeatureTest, TestHandlerName) {
  std::optional<std::string> handler_name =
      feature_->GetScriptMessageHandlerName();
  ASSERT_TRUE(handler_name.has_value());
  EXPECT_EQ(*handler_name, "ClientSideDetectionMessage");
}
