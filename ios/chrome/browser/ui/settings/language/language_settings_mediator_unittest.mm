// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"

#import <memory>
#import <string>
#import <vector>

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/language/core/browser/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/language/model/language_model_manager_factory.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_consumer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_collator.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using language::prefs::kAcceptLanguages;
using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;
using ::testing::ElementsAreArray;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

std::vector<std::u16string> ExtractDisplayNamesFromLanguageItems(
    NSArray<LanguageItem*>* language_items) {
  __block std::vector<std::u16string> output;
  [language_items enumerateObjectsUsingBlock:^(LanguageItem* item,
                                               NSUInteger index, BOOL* stop) {
    output.push_back(base::SysNSStringToUTF16(item.text));
  }];
  return output;
}

std::vector<std::string> ExtractLanguageCodesFromLanguageItems(
    NSArray<LanguageItem*>* language_items) {
  __block std::vector<std::string> output;
  [language_items enumerateObjectsUsingBlock:^(LanguageItem* item,
                                               NSUInteger index, BOOL* stop) {
    output.push_back(item.languageCode);
  }];
  return output;
}

}  // namespace

// Test class that conforms to LanguageSettingsConsumer in order to test the
// consumer methods are called correctly.
@interface FakeLanguageSettingsConsumer : NSObject <LanguageSettingsConsumer>

@property(nonatomic, assign) BOOL translateEnabled;
@property(nonatomic, assign) BOOL translateEnabledWasCalled;
@property(nonatomic, assign) BOOL languagePrefsChangedWasCalled;

@end

@implementation FakeLanguageSettingsConsumer

- (void)translateEnabled:(BOOL)enabled {
  self.translateEnabled = enabled;
  self.translateEnabledWasCalled = YES;
}

- (void)languagePrefsChanged {
  self.languagePrefsChangedWasCalled = YES;
}

@end

class LanguageSettingsMediatorTest : public PlatformTest {
 protected:
  LanguageSettingsMediatorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    // Create profile.
    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();

    // Create TranslatePrefs.
    translate_prefs_ =
        ChromeIOSTranslateClient::CreateTranslatePrefs(profile_->GetPrefs());

    // Make sure the accept languages list is empty.
    std::vector<std::string> languages;
    translate_prefs_->GetLanguageList(&languages);
    for (const auto& language : languages) {
      translate_prefs_->RemoveFromLanguageList(language);
    }

    consumer_ = [[FakeLanguageSettingsConsumer alloc] init];
    language::LanguageModelManager* language_model_manager =
        LanguageModelManagerFactory::GetForProfile(profile_.get());

    mediator_ = [[LanguageSettingsMediator alloc]
        initWithLanguageModelManager:language_model_manager
                         prefService:GetPrefs()];
    mediator_.consumer = consumer_;
  }

  ~LanguageSettingsMediatorTest() override { [mediator_ stopObservingModel]; }

  PrefService* GetPrefs() { return profile_->GetPrefs(); }

  translate::TranslatePrefs* translate_prefs() {
    return translate_prefs_.get();
  }

  FakeLanguageSettingsConsumer* consumer() { return consumer_; }

  LanguageSettingsMediator* mediator() { return mediator_; }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<PrefRegistrySyncable> registry = new PrefRegistrySyncable();
    // Registers Translate and Language related prefs.
    RegisterProfilePrefs(registry.get());
    PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;
  FakeLanguageSettingsConsumer* consumer_;
  LanguageSettingsMediator* mediator_;
};

// Tests that the mediator notifies its consumer when the value of
// translate::prefs::kOfferTranslateEnabled, language::prefs::kAcceptLanguages
// or translate::prefs::kBlockedLanguages change.
TEST_F(LanguageSettingsMediatorTest, TestPrefsChanged) {
  consumer().translateEnabledWasCalled = NO;
  EXPECT_FALSE([consumer() translateEnabled]);
  GetPrefs()->SetBoolean(translate::prefs::kOfferTranslateEnabled, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kSyncOperationTimeout, ^bool() {
    return consumer().translateEnabledWasCalled;
  }));
  EXPECT_TRUE([consumer() translateEnabled]);

  consumer().translateEnabledWasCalled = NO;
  EXPECT_TRUE([consumer() translateEnabled]);
  GetPrefs()->SetBoolean(translate::prefs::kOfferTranslateEnabled, false);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kSyncOperationTimeout, ^bool() {
    return consumer().translateEnabledWasCalled;
  }));
  EXPECT_FALSE([consumer() translateEnabled]);

  consumer().languagePrefsChangedWasCalled = NO;
  EXPECT_FALSE(translate_prefs()->IsBlockedLanguage("fa"));
  translate_prefs()->AddToLanguageList("fa", /*force_blocked=*/false);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kSyncOperationTimeout, ^bool() {
    return consumer().languagePrefsChangedWasCalled;
  }));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));

  consumer().languagePrefsChangedWasCalled = NO;
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));
  translate_prefs()->UnblockLanguage("fa");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kSyncOperationTimeout, ^bool() {
    return consumer().languagePrefsChangedWasCalled;
  }));
  EXPECT_FALSE(translate_prefs()->IsBlockedLanguage("fa"));
}

// Tests that the list of supported language items are sorted by display names
// and excludes languages already in the accept languages list.
TEST_F(LanguageSettingsMediatorTest, TestSupportedLanguagesItems) {
  NSArray<LanguageItem*>* language_items = [mediator() supportedLanguagesItems];
  std::vector<std::u16string> display_names =
      ExtractDisplayNamesFromLanguageItems(language_items);
  std::vector<std::u16string> sorted(display_names);
  l10n_util::SortVectorWithStringKey("en-US", &sorted, false);
  EXPECT_THAT(display_names, ElementsAreArray(sorted));

  std::vector<std::string> language_codes =
      ExtractLanguageCodesFromLanguageItems(language_items);
  EXPECT_TRUE(base::Contains(language_codes, "fa"));

  translate_prefs()->AddToLanguageList("fa", /*force_blocked=*/false);
  language_items = [mediator() supportedLanguagesItems];
  language_codes = ExtractLanguageCodesFromLanguageItems(language_items);
  EXPECT_FALSE(base::Contains(language_codes, "fa"));
}

// Tests that the list of accept language items is as expected.
TEST_F(LanguageSettingsMediatorTest, TestAcceptLanguagesItems) {
  translate_prefs()->AddToLanguageList("fa", /*force_blocked=*/false);
  translate_prefs()->AddToLanguageList("en-US", /*force_blocked=*/false);
  translate_prefs()->AddToLanguageList("to", /*force_blocked=*/false);
  translate_prefs()->SetRecentTargetLanguage("fa");
  translate_prefs()->UnblockLanguage("en-US");

  NSArray<LanguageItem*>* acceptLanguagesItems =
      [mediator() acceptLanguagesItems];
  ASSERT_EQ(3U, [acceptLanguagesItems count]);

  EXPECT_EQ("fa", acceptLanguagesItems[0].languageCode);
  EXPECT_TRUE(acceptLanguagesItems[0].supportsTranslate);
  EXPECT_TRUE(acceptLanguagesItems[0].targetLanguage);
  EXPECT_TRUE(acceptLanguagesItems[0].blocked);

  EXPECT_EQ("en-US", acceptLanguagesItems[1].languageCode);
  EXPECT_TRUE(acceptLanguagesItems[1].supportsTranslate);
  EXPECT_FALSE(acceptLanguagesItems[1].targetLanguage);
  EXPECT_FALSE(acceptLanguagesItems[1].blocked);

  EXPECT_EQ("to", acceptLanguagesItems[2].languageCode);
  EXPECT_FALSE(acceptLanguagesItems[2].supportsTranslate);
  EXPECT_FALSE(acceptLanguagesItems[2].targetLanguage);
  EXPECT_TRUE(acceptLanguagesItems[2].blocked);
}

// Tests that the mediator updates the model upon receiving the UI commands.
TEST_F(LanguageSettingsMediatorTest, TestLanguageSettingsCommands) {
  [mediator() setTranslateEnabled:NO];
  EXPECT_FALSE(
      GetPrefs()->GetBoolean(translate::prefs::kOfferTranslateEnabled));

  [mediator() setTranslateEnabled:YES];
  EXPECT_TRUE(GetPrefs()->GetBoolean(translate::prefs::kOfferTranslateEnabled));

  [mediator() addLanguage:"fa"];
  [mediator() addLanguage:"en-US"];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("en-US"));

  [mediator() unblockLanguage:"en-US"];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));
  EXPECT_FALSE(translate_prefs()->IsBlockedLanguage("en-US"));

  // The last fluent language cannot be unblocked.
  [mediator() unblockLanguage:"fa"];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));
  EXPECT_FALSE(translate_prefs()->IsBlockedLanguage("en-US"));

  [mediator() blockLanguage:"en-US"];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("fa"));
  EXPECT_TRUE(translate_prefs()->IsBlockedLanguage("en-US"));

  [mediator() moveLanguage:"fa" downward:YES withOffset:1];
  EXPECT_EQ("en-US,fa", GetPrefs()->GetString(kAcceptLanguages));

  [mediator() moveLanguage:"fa" downward:NO withOffset:1];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));

  // Moving the first language up in order has no effect.
  [mediator() moveLanguage:"fa" downward:NO withOffset:1];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));

  // Moving the last language down in order has no effect.
  [mediator() moveLanguage:"en-US" downward:YES withOffset:1];
  EXPECT_EQ("fa,en-US", GetPrefs()->GetString(kAcceptLanguages));

  [mediator() removeLanguage:"fa"];
  EXPECT_EQ("en-US", GetPrefs()->GetString(kAcceptLanguages));

  [mediator() removeLanguage:"en-US"];
  EXPECT_EQ("", GetPrefs()->GetString(kAcceptLanguages));
}
