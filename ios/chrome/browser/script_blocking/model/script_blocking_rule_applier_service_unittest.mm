// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "ios/web/public/test/fakes/fake_content_rule_list_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for ScriptBlockingRuleApplierService.
class ScriptBlockingRuleApplierServiceTest : public PlatformTest {
 public:
  // Wrapper to call the private `OnScriptBlockingRuleListUpdated` method on the
  // service. This is necessary because the `TEST_F` macro creates a subclass
  // for the test body, which would not have access to private members of
  // `ScriptBlockingRuleApplierService`.
  void CallOnScriptBlockingRuleListUpdated(const std::string& rules_json) {
    service_->OnScriptBlockingRuleListUpdated(rules_json);
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    service_ = std::make_unique<ScriptBlockingRuleApplierService>(
        fake_content_rule_list_manager_);
  }

  void TearDown() override {
    service_->Shutdown();
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeContentRuleListManager fake_content_rule_list_manager_;
  std::unique_ptr<ScriptBlockingRuleApplierService> service_;
};

// Tests that a non-empty rule list JSON triggers an update.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestUpdateRuleList) {
  const std::string json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  CallOnScriptBlockingRuleListUpdated(json);

  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_json(), json);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_remove_key().empty());
}

// Tests that an empty rule list JSON triggers a removal.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestRemoveRuleList) {
  CallOnScriptBlockingRuleListUpdated("");

  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_json().empty());
}

// Tests that the completion callback is invoked with an error.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestCompletionCallbackWithError) {
  CallOnScriptBlockingRuleListUpdated("[]");

  // The service should not crash when the callback is invoked with an error.
  // This test primarily verifies that the callback handling logic is present
  // and doesn't cause issues.
  NSError* error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  fake_content_rule_list_manager_.InvokeCompletionCallback(error);
  // No crash is a pass.
}

// Tests that the completion callback is invoked without an error.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCompletionCallbackWithoutError) {
  CallOnScriptBlockingRuleListUpdated("[]");

  // The service should not crash when the callback is invoked without an
  // error.
  fake_content_rule_list_manager_.InvokeCompletionCallback(nil);
  // No crash is a pass.
}
