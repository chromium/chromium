// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using language::prefs::kAcceptLanguages;
using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;

std::vector<std::string> ExtractDisplayNamesFromLanguageItems(
    NSArray<LanguageItem*>* language_items) {
  __block std::vector<std::string> output;
  [language_items enumerateObjectsUsingBlock:^(LanguageItem* item,
                                               NSUInteger index, BOOL* stop) {
    output.push_back(base::SysNSStringToUTF8(item.text));
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
    // Create BrowserState.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();

    // Create TranslatePrefs.
    translate_prefs_ = ChromeIOSTranslateClient::CreateTranslatePrefs(
        chrome_browser_state_->GetPrefs());

    // Make sure the accept languages list is empty.
    std::vector<std::string> languages;
    translate_prefs_->GetLanguageList(&languages);
    for (const auto& language : languages) {
      translate_prefs_->RemoveFromLanguageList(language);
    }

    consumer_ = [[FakeLanguageSettingsConsumer alloc] init];

    mediator_ = [[LanguageSettingsMediator alloc]
        initWithBrowserState:chrome_browser_state_.get()];
    mediator_.consumer = consumer_;
  }

  ~LanguageSettingsMediatorTest() override { [mediator_ stopObservingModel]; }

  PrefService* GetPrefs() { return chrome_browser_state_->GetPrefs(); }

  translate::TranslatePrefs* translate_prefs() {
    return translate_prefs_.get();
  }

  FakeLanguageSettingsConsumer* consumer() { return consumer_; }

  LanguageSettingsMediator* mediator() { return mediator_; }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<PrefRegistrySyncable> registry = new PrefRegistrySyncable();
    // Registers Translate and Language related prefs.
    RegisterBrowserStatePrefs(registry.get());
    PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;
  FakeLanguageSettingsConsumer* consumer_;
  LanguageSettingsMediator* mediator_;
};

// Tests that the mediator notifies its consumer when the value of
// prefs::kOfferTranslateEnabled, language::prefs::kAcceptLanguages or
// language::prefs::kFluentLanguages change.
TEST_F(LanguageSettingsMediatorTest, TestPrefsChanged) {
  consumer().translateEnabledWasCalled = NO;
  EXPECT_FALSE([consumer() translateEnabled]);
  GetPrefs()->SetBoolean(prefs::kOfferTranslateEnabled, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kSyncOperationTimeout, ^bool() {
    return consumer().translateEnabledWasCalled;
  }));
  EXPECT_TRUE([consumer() translateEnabled]);

  consumer().translateEnabledWasCalled = NO;
  EXPECT_TRUE([consumer() translateEnabled]);
  GetPrefs()->SetBoolean(prefs::kOfferTranslateEnabled, false);
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
  std::vector<std::string> display_names =
      ExtractDisplayNamesFromLanguageItems(language_items);
  EXPECT_TRUE(std::is_sorted(display_names.begin(), display_names.end()));

  std::vector<std::string> language_codes =
      ExtractLanguageCodesFromLanguageItems(language_items);
  EXPECT_TRUE(std::find(language_codes.begin(), language_codes.end(), "fa") !=
              language_codes.end());

  translate_prefs()->AddToLanguageList("fa", /*force_blocked=*/false);
  language_items = [mediator() supportedLanguagesItems];
  language_codes = ExtractLanguageCodesFromLanguageItems(language_items);
  EXPECT_FALSE(std::find(language_codes.begin(), language_codes.end(), "fa") !=
               language_codes.end());
}

// Tests that the list of accept language items is as expected.
TEST_F(LanguageSettingsMediatorTest, TestAcceptLanguagesItems) {
  translate_prefs()->AddToLanguageList("fa", /*force_blocked=*/false);
  translate_prefs()->AddToLanguageList("en-US", /*force_blocked=*/false);
  translate_prefs()->AddToLanguageList("ug", /*force_blocked=*/false);
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

  EXPECT_EQ("ug", acceptLanguagesItems[2].languageCode);
  EXPECT_FALSE(acceptLanguagesItems[2].supportsTranslate);
  EXPECT_FALSE(acceptLanguagesItems[2].targetLanguage);
  EXPECT_TRUE(acceptLanguagesItems[2].blocked);
}

// Tests that the mediator updates the model upon receiving the UI commands.
TEST_F(LanguageSettingsMediatorTest, TestLanguageSettingsCommands) {
  [mediator() setTranslateEnabled:NO];
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kOfferTranslateEnabled));

  [mediator() setTranslateEnabled:YES];
  EXPECT_TRUE(GetPrefs()->GetBoolean(prefs::kOfferTranslateEnabled));

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
