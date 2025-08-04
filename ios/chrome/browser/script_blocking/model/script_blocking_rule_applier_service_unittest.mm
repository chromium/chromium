// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/content_settings/core/common/pref_names.h"
#import "components/fingerprinting_protection_filter/browser/test_support.h"
#import "components/privacy_sandbox/privacy_sandbox_features.h"
#import "ios/web/public/test/fakes/fake_content_rule_list_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
// Helper function to generate the expected JSON output when an exception is
// added to a base rule list.
std::string CreateExpectedJsonWithException(const std::string& base_json,
                                            const GURL& exception_url) {
  std::optional<base::Value> value = base::JSONReader::Read(base_json);
  CHECK(value.has_value());
  base::Value::List list = std::move(value->GetList());
  base::Value::Dict exception_rule;
  exception_rule.SetByDottedPath("action.type", "ignore-previous-rules");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(exception_url);
  exception_rule.SetByDottedPath(
      "trigger.if-domain", base::Value::List().Append("*" + pattern.GetHost()));
  exception_rule.SetByDottedPath("trigger.url-filter", ".*");
  list.Append(std::move(exception_rule));
  std::string expected_json;
  base::JSONWriter::Write(list, &expected_json);
  return expected_json;
}
}  // namespace

// Test fixture for ScriptBlockingRuleApplierService.
class ScriptBlockingRuleApplierServiceTest : public PlatformTest {
 public:
  ScriptBlockingRuleApplierServiceTest() {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kFingerprintingProtectionUx);
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    script_blocking::ContentRuleListData::GetInstance().ResetForTesting();
    test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                      true);
    incognito_tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            test_support_.prefs(), test_support_.content_settings(),
            /*management_service=*/nullptr, /*is_incognito=*/true);
    service_ = std::make_unique<ScriptBlockingRuleApplierService>(
        fake_content_rule_list_manager_,
        incognito_tracking_protection_settings_.get());
    // Clear any state set by the service's constructor.
    fake_content_rule_list_manager_.Clear();
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  fingerprinting_protection_filter::TestSupport test_support_;
  web::FakeContentRuleListManager fake_content_rule_list_manager_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      incognito_tracking_protection_settings_;
  std::unique_ptr<ScriptBlockingRuleApplierService> service_;
};

// Tests that a non-empty rule list JSON triggers an update.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestUpdateRuleList) {
  const std::string json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";

  // The service will re-serialize the JSON, so we must compare against the
  // canonical representation.
  std::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value.has_value());
  std::string expected_json;
  base::JSONWriter::Write(*value, &expected_json);

  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(json);

  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_json(), expected_json);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_remove_key().empty());
}

// Tests toggling the FP protection pref off and on.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestTogglingFpProtectionPref) {
  const std::string json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(json);

  // Verify the rules were added initially.
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_FALSE(fake_content_rule_list_manager_.last_update_json().empty());
  fake_content_rule_list_manager_.Clear();

  // Disable the pref. The service should observe this and remove the rules.
  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    false);

  // Verify the rules were removed.
  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
  fake_content_rule_list_manager_.Clear();

  // Re-enable the pref. The service should observe this and re-apply the rules.
  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    true);

  // Verify the rules were re-added.
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_FALSE(fake_content_rule_list_manager_.last_update_json().empty());
}

// Tests that adding an exception does nothing if the base rule list is empty.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestExceptionWithEmptyRuleList) {
  // Adding the exception triggers an automatic update. Because the base rule
  // list is empty, the service should remove any existing list.
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      GURL("https://example.com"));

  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
}

// Tests that the service correctly updates rules when an exception is added.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestExceptionAddedWhenRulesAreApplied) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);
  fake_content_rule_list_manager_.Clear();

  // Add an exception. The service should observe this and update the rules.
  GURL exception_url("https://example.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  std::string expected_json =
      CreateExpectedJsonWithException(base_json, exception_url);

  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_json(), expected_json);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_remove_key().empty());
}
// Tests that an empty rule list JSON triggers a removal.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestRemoveRuleList) {
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList("");

  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_json().empty());
}

// Tests that the completion callback is invoked with an error.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestCompletionCallbackWithError) {
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList("[]");

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
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList("[]");

  // The service should not crash when the callback is invoked without an
  // error.
  fake_content_rule_list_manager_.InvokeCompletionCallback(nil);
  // No crash is a pass.
}

// Tests that the service removes the rule list when the fingerprinting
// protection pref is disabled.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestFpProtectionPrefDisabled) {
  const std::string json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(json);
  fake_content_rule_list_manager_.Clear();

  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    false);

  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_json().empty());
}

// Tests that the service correctly adds exception rules.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestWithTrackingProtectionException) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://example.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  std::string expected_json =
      CreateExpectedJsonWithException(base_json, exception_url);

  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_json(), expected_json);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_remove_key().empty());
}
