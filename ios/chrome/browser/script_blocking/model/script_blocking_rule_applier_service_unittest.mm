// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/script_blocking/model/script_blocking_rule_applier_service.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/functional/callback.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/content_settings/core/common/pref_names.h"
#import "components/fingerprinting_protection_filter/browser/test_support.h"
#import "components/privacy_sandbox/privacy_sandbox_features.h"
#import "ios/web/public/test/fakes/fake_content_rule_list_manager.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Helper function to parse a JSON string and return the last rule in the list.
base::Value::Dict GetLastRule(const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value.has_value());
  const base::Value::List* list = value->GetIfList();
  CHECK(list);
  CHECK(!list->empty());
  const base::Value::Dict* last_rule = list->back().GetIfDict();
  CHECK(last_rule);
  return last_rule->Clone();
}

MATCHER_P(IsExceptionRule, expected_top_url, "") {
  const base::Value::Dict* action = arg.FindDict("action");
  if (!action) {
    return false;
  }
  const std::string* type = action->FindString("type");
  if (!type || *type != "ignore-previous-rules") {
    return false;
  }

  const base::Value::Dict* trigger = arg.FindDict("trigger");
  if (!trigger) {
    return false;
  }
  const std::string* url_filter = trigger->FindString("url-filter");
  if (!url_filter || *url_filter != ".*") {
    return false;
  }

  const base::Value::List* if_top_url_list = trigger->FindList("if-top-url");
  if (!if_top_url_list || if_top_url_list->size() != 1) {
    return false;
  }
  const std::string* top_url = (*if_top_url_list)[0].GetIfString();
  if (!top_url || *top_url != expected_top_url) {
    return false;
  }

  return true;
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
  // Reset state before testing the pref change.
  fake_content_rule_list_manager_.Clear();

  // Disable the pref. The service should observe this and remove the rules.
  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    false);

  // Verify the rules were removed.
  EXPECT_EQ(fake_content_rule_list_manager_.last_remove_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_TRUE(fake_content_rule_list_manager_.last_update_key().empty());
  // Reset state before re-enabling the pref.
  fake_content_rule_list_manager_.Clear();

  // Re-enable the pref. The service should observe this and re-apply the rules.
  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    true);

  // Verify the rules were re-added.
  EXPECT_EQ(fake_content_rule_list_manager_.last_update_key(),
            ScriptBlockingRuleApplierService::kScriptBlockingRuleListKey);
  EXPECT_FALSE(fake_content_rule_list_manager_.last_update_json().empty());
}

// Tests that the service correctly adds exception rules with the correct regex.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestCorrectExceptionRule) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://foo.example.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
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

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules with the correct regex
// for a domain on the public suffix list.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithPublicSuffix) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://sub.example.co.uk");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(
      last_rule,
      IsExceptionRule("^https://(?:[^/.]*\\.)*example\\.co\\.uk(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for a URL with a port.
TEST_F(ScriptBlockingRuleApplierServiceTest, TestCorrectExceptionRuleWithPort) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://example.com:8080");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for an IP address.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithIPAddress) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://192.168.0.1");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*192\\.168\\.0\\.1(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for a domain with a
// hyphen.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithHyphen) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://foo-bar.example.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for a domain with
// multiple subdomains.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithMultipleSubdomains) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://a.b.c.example.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for a domain with
// mixed-case characters.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithMixedCase) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://Foo.Example.Com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule("^https://"
                              "(?:[^/.]*\\.)*example\\.com(?:/.*)?$"));
}

// Tests that the service correctly adds exception rules for a domain with
// special characters that need escaping.
TEST_F(ScriptBlockingRuleApplierServiceTest,
       TestCorrectExceptionRuleWithSpecialCharacters) {
  const std::string base_json = "[{\"trigger\": {\"url-filter\": \".*\"}}]";
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(
      base_json);

  // Clear state from setup to isolate the test action.
  fake_content_rule_list_manager_.Clear();

  GURL exception_url("https://sub.example-with-special-chars.com");
  incognito_tracking_protection_settings_->AddTrackingProtectionException(
      exception_url);

  const base::Value::Dict last_rule =
      GetLastRule(fake_content_rule_list_manager_.last_update_json());
  EXPECT_THAT(last_rule,
              IsExceptionRule(
                  "^https://(?:[^/"
                  ".]*\\.)*example\\-with\\-special\\-chars\\.com(?:/.*)?$"));
}
