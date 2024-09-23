// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_store.h"

#include <stdint.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/features.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api::types::ChromeSettingScope;
using ::testing::Mock;

namespace extensions {

namespace {

void CheckRule(std::unique_ptr<content_settings::Rule> rule,
               const ContentSettingsPattern& primary_pattern,
               const ContentSettingsPattern& secondary_pattern,
               ContentSetting setting) {
  EXPECT_EQ(primary_pattern.ToString(), rule->primary_pattern.ToString());
  EXPECT_EQ(secondary_pattern.ToString(), rule->secondary_pattern.ToString());
  EXPECT_EQ(setting, content_settings::ValueToContentSetting(rule->value));
}

// Helper class which returns monotonically-increasing base::Time objects.
class FakeTimer {
 public:
  FakeTimer() = default;

  base::Time GetNext() {
    return base::Time::FromInternalValue(++internal_);
  }

 private:
  int64_t internal_ = 0;
};

class MockContentSettingsStoreObserver
    : public ContentSettingsStore::Observer {
 public:
  MOCK_METHOD2(OnContentSettingChanged,
               void(const ExtensionId& extension_id, bool incognito));
};

ContentSetting GetContentSettingFromStore(
    const ContentSettingsStore* store,
    const GURL& primary_url, const GURL& secondary_url,
    ContentSettingsType content_type,
    bool incognito) {
  auto rule =
      store->GetRule(primary_url, secondary_url, content_type, incognito);

  return rule ? content_settings::ValueToContentSetting(rule->value)
              : CONTENT_SETTING_DEFAULT;
}

std::vector<std::unique_ptr<content_settings::Rule>>
GetSettingsForOneTypeFromStore(const ContentSettingsStore* store,
                               ContentSettingsType content_type,
                               bool incognito) {
  std::vector<std::unique_ptr<content_settings::Rule>> rules;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      store->GetRuleIterator(content_type, incognito));
  if (rule_iterator) {
    while (rule_iterator->HasNext())
      rules.push_back(rule_iterator->Next());
  }
  return rules;
}

}  // namespace

class ContentSettingsStoreTest : public ::testing::Test {
 public:
  ContentSettingsStoreTest() :
      store_(new ContentSettingsStore()) {
  }

 protected:
  void RegisterExtension(const std::string& ext_id) {
    store_->RegisterExtension(ext_id, timer_.GetNext(), true);
  }

  ContentSettingsStore* store() {
    return store_.get();
  }

 private:
  FakeTimer timer_;
  scoped_refptr<ContentSettingsStore> store_;
};

TEST_F(ContentSettingsStoreTest, RegisterUnregister) {
  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  // Register first extension
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  // Set setting
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->SetExtensionContentSetting(
      ext_id, pattern, pattern, ContentSettingsType::COOKIES,
      CONTENT_SETTING_ALLOW, ChromeSettingScope::kRegular);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->SetExtensionContentSetting(
      ext_id_2, pattern, pattern, ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK, ChromeSettingScope::kRegular);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  // Unregister first extension. This shouldn't change the setting.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->UnregisterExtension(ext_id);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));
  Mock::VerifyAndClear(&observer);

  // Unregister second extension. This should reset the setting to its default
  // value.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->UnregisterExtension(ext_id_2);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  store()->RemoveObserver(&observer);
}

TEST_F(ContentSettingsStoreTest, GetAllSettings) {
  const bool incognito = false;
  std::vector<std::unique_ptr<content_settings::Rule>> rules =
      GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                     incognito);
  ASSERT_EQ(0u, rules.size());
  rules.clear();

  // Register first extension.
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  store()->SetExtensionContentSetting(
      ext_id, pattern, pattern, ContentSettingsType::COOKIES,
      CONTENT_SETTING_ALLOW, ChromeSettingScope::kRegular);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         incognito);
  ASSERT_EQ(1u, rules.size());
  CheckRule(std::move(rules[0]), pattern, pattern, CONTENT_SETTING_ALLOW);
  rules.clear();

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromURL(GURL("http://www.example.com"));
  store()->SetExtensionContentSetting(
      ext_id_2, pattern_2, pattern_2, ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK, ChromeSettingScope::kRegular);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         incognito);
  ASSERT_EQ(2u, rules.size());
  // Rules appear in the reverse installation order of the extensions.
  CheckRule(std::move(rules[0]), pattern_2, pattern_2, CONTENT_SETTING_BLOCK);
  CheckRule(std::move(rules[1]), pattern, pattern, CONTENT_SETTING_ALLOW);
  rules.clear();

  // Disable first extension.
  store()->SetExtensionState(ext_id, false);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         incognito);
  ASSERT_EQ(1u, rules.size());
  CheckRule(std::move(rules[0]), pattern_2, pattern_2, CONTENT_SETTING_BLOCK);
  rules.clear();

  // Uninstall second extension.
  store()->UnregisterExtension(ext_id_2);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         incognito);
  ASSERT_EQ(0u, rules.size());
}

TEST_F(ContentSettingsStoreTest, SetFromList) {
  // Force creation of ContentSettingsRegistry, so that the string to content
  // setting type lookup can succeed.
  content_settings::ContentSettingsRegistry::GetInstance();

  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  // Register first extension
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  // Set setting via a list
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));

  // Build a preference list in JSON format.
  base::Value::List pref_list;
  // {"primaryPattern": pattern, "secondaryPattern": pattern, "type": "cookies",
  //  "setting": "allow"}
  base::Value::Dict dict_value;
  dict_value.Set(ContentSettingsStore::kPrimaryPatternKey, pattern.ToString());
  dict_value.Set(ContentSettingsStore::kSecondaryPatternKey,
                 pattern.ToString());
  dict_value.Set(ContentSettingsStore::kContentSettingsTypeKey, "cookies");
  dict_value.Set(ContentSettingsStore::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));
  // Test content settings types that have been removed. Should be ignored.
  // {"primaryPattern": pattern, "secondaryPattern": pattern,
  //  "type": "fullscreen", "setting": "allow"}
  dict_value = base::Value::Dict();
  dict_value.Set(ContentSettingsStore::kPrimaryPatternKey, pattern.ToString());
  dict_value.Set(ContentSettingsStore::kSecondaryPatternKey,
                 pattern.ToString());
  dict_value.Set(ContentSettingsStore::kContentSettingsTypeKey, "fullscreen");
  dict_value.Set(ContentSettingsStore::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));
  // {"primaryPattern": pattern, "secondaryPattern": pattern,
  //  "type": "mouselock", "setting": "allow"}
  dict_value = base::Value::Dict();
  dict_value.Set(ContentSettingsStore::kPrimaryPatternKey, pattern.ToString());
  dict_value.Set(ContentSettingsStore::kSecondaryPatternKey,
                 pattern.ToString());
  dict_value.Set(ContentSettingsStore::kContentSettingsTypeKey, "mouselock");
  dict_value.Set(ContentSettingsStore::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  store()->SetExtensionContentSettingFromList(ext_id, pref_list,
                                              ChromeSettingScope::kRegular);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES, false));

  store()->RemoveObserver(&observer);
}

// Test that embedded patterns are properly removed.
TEST_F(ContentSettingsStoreTest, RemoveEmbedded) {
  content_settings::ContentSettingsRegistry::GetInstance();

  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL primary_url("http://www.youtube.com");
  GURL secondary_url("http://www.google.com");

  // Register first extension.
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  // Set setting via a list.
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURL(primary_url);
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromURL(secondary_url);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false)).Times(1);

  // Build a preference list in JSON format.
  base::Value::List pref_list;
  base::Value::Dict dict_value;
  dict_value.Set(ContentSettingsStore::kPrimaryPatternKey,
                 primary_pattern.ToString());
  dict_value.Set(ContentSettingsStore::kSecondaryPatternKey,
                 secondary_pattern.ToString());
  dict_value.Set(ContentSettingsStore::kContentSettingsTypeKey, "cookies");
  dict_value.Set(ContentSettingsStore::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  dict_value = base::Value::Dict();
  dict_value.Set(ContentSettingsStore::kPrimaryPatternKey,
                 primary_pattern.ToString());
  dict_value.Set(ContentSettingsStore::kSecondaryPatternKey,
                 secondary_pattern.ToString());
  dict_value.Set(ContentSettingsStore::kContentSettingsTypeKey, "geolocation");
  dict_value.Set(ContentSettingsStore::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  store()->SetExtensionContentSettingFromList(ext_id, pref_list,
                                              ChromeSettingScope::kRegular);

  // The embedded geolocation pattern should be removed but cookies kept.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSettingFromStore(store(), primary_url, secondary_url,
                                       ContentSettingsType::COOKIES, false));
  EXPECT_EQ(
      CONTENT_SETTING_DEFAULT,
      GetContentSettingFromStore(store(), primary_url, secondary_url,
                                 ContentSettingsType::GEOLOCATION, false));

  Mock::VerifyAndClear(&observer);
  store()->RemoveObserver(&observer);
}

TEST_F(ContentSettingsStoreTest, ChromeExtensionSchemeMetrics) {
  base::HistogramTester histogram_tester;
  content_settings::ContentSettingsRegistry::GetInstance();
  std::string extension_id(32, 'a');
  ContentSettingsPattern chrome_extension_pattern =
      ContentSettingsPattern::FromString(
          "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel/");
  ContentSettingsPattern https_pattern =
      ContentSettingsPattern::FromString("https://example.test/");

  RegisterExtension(extension_id);
  store()->SetExtensionContentSetting(
      extension_id, chrome_extension_pattern, https_pattern,
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW,
      ChromeSettingScope::kRegular);
  histogram_tester.ExpectUniqueSample(
      "Extensions.ContentSettings.PrimaryPatternChromeExtensionScheme",
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::COOKIES),
      1);

  RegisterExtension(extension_id);
  store()->SetExtensionContentSetting(
      extension_id, https_pattern, chrome_extension_pattern,
      ContentSettingsType::IMAGES, CONTENT_SETTING_ALLOW,
      ChromeSettingScope::kRegular);
  histogram_tester.ExpectUniqueSample(
      "Extensions.ContentSettings.SecondaryPatternChromeExtensionScheme",
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::IMAGES),
      1);
}

TEST_F(ContentSettingsStoreTest, SetExtensionContentSettingFromList) {
  content_settings::ContentSettingsRegistry::GetInstance();

  std::string extension = "extension_id";
  RegisterExtension(extension);

  base::Value::Dict valid_setting;
  valid_setting.Set(ContentSettingsStore::kPrimaryPatternKey,
                    "http://example1.com");
  valid_setting.Set(ContentSettingsStore::kSecondaryPatternKey, "*");
  valid_setting.Set(ContentSettingsStore::kContentSettingsTypeKey,
                    "javascript");
  valid_setting.Set(ContentSettingsStore::kContentSettingKey, "allow");

  // Missing secondary key.
  base::Value::Dict invalid_setting1;
  invalid_setting1.Set(ContentSettingsStore::kPrimaryPatternKey,
                       "http://example2.com");
  invalid_setting1.Set(ContentSettingsStore::kContentSettingsTypeKey,
                       "javascript");
  invalid_setting1.Set(ContentSettingsStore::kContentSettingKey, "allow");

  // Invalid secondary pattern.
  base::Value::Dict invalid_setting2;
  invalid_setting2.Set(ContentSettingsStore::kPrimaryPatternKey,
                       "http://example3.com");
  invalid_setting2.Set(ContentSettingsStore::kSecondaryPatternKey, "[*.].");
  invalid_setting2.Set(ContentSettingsStore::kContentSettingsTypeKey,
                       "javascript");
  invalid_setting2.Set(ContentSettingsStore::kContentSettingKey, "allow");

  // Invalid setting.
  base::Value::Dict invalid_setting3;
  invalid_setting3.Set(ContentSettingsStore::kPrimaryPatternKey,
                       "http://example4.com");
  invalid_setting3.Set(ContentSettingsStore::kSecondaryPatternKey, "*");
  invalid_setting3.Set(ContentSettingsStore::kContentSettingsTypeKey,
                       "javascript");
  invalid_setting3.Set(ContentSettingsStore::kContentSettingKey, "notasetting");

  base::Value::List list;
  list.Append(valid_setting.Clone());
  list.Append(invalid_setting1.Clone());
  list.Append(invalid_setting2.Clone());
  list.Append(invalid_setting3.Clone());
  store()->SetExtensionContentSettingFromList(extension, list,
                                              ChromeSettingScope::kRegular);

  base::Value::List expected;
  expected.Append(valid_setting.Clone());
  EXPECT_EQ(expected, store()->GetSettingsForExtension(
                          extension, ChromeSettingScope::kRegular));
}

}  // namespace extensions
