// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/translate_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/task_environment.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_locale_settings.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using user_prefs::PrefRegistrySyncable;

namespace {

const char kBlockedSite[] = "http://blockedsite.com";
const char kLanguage1[] = "af";
const char kLanguage2[] = "fr";

class TranslateTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  TranslateTableViewControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[TranslateTableViewController alloc]
        initWithPrefs:pref_service_.get()];
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    scoped_refptr<PrefRegistrySyncable> registry = new PrefRegistrySyncable();
    registry->RegisterBooleanPref(translate::prefs::kOfferTranslateEnabled,
                                  false, PrefRegistrySyncable::SYNCABLE_PREF);
    language::LanguagePrefs::RegisterProfilePrefs(registry.get());
    translate::TranslatePrefs::RegisterProfilePrefs(registry.get());
    base::FilePath path("TranslateTableViewControllerTest.pref");
    sync_preferences::PrefServiceMockFactory factory;
    factory.SetUserPrefsFile(
        path, base::SingleThreadTaskRunner::GetCurrentDefault().get());
    return factory.Create(registry.get());
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<TranslateTableViewController>(controller())
        settingsWillBeDismissed];
    LegacyChromeTableViewControllerTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> pref_service_;
};

TEST_F(TranslateTableViewControllerTest, TestModelTranslateOff) {
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(NO, IDS_IOS_TRANSLATE_SETTING, 0, 0);
  CheckTextCellTextWithId(IDS_IOS_TRANSLATE_SETTING_RESET, 0, 1);
}

TEST_F(TranslateTableViewControllerTest, TestModelTranslateOn) {
  BooleanPrefMember translateEnabled;
  translateEnabled.Init(translate::prefs::kOfferTranslateEnabled,
                        pref_service_.get());
  translateEnabled.SetValue(true);
  CreateController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTextWithId(YES, IDS_IOS_TRANSLATE_SETTING, 0, 0);
  CheckTextCellTextWithId(IDS_IOS_TRANSLATE_SETTING_RESET, 0, 1);
}

TEST_F(TranslateTableViewControllerTest, TestClearPreferences) {
  // Set some preferences.
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(pref_service_.get()));
  translate_prefs->AddSiteToNeverPromptList(kBlockedSite);
  ASSERT_TRUE(translate_prefs->IsSiteOnNeverPromptList(kBlockedSite));
  translate_prefs->AddToLanguageList(kLanguage1, /*force_blocked=*/true);
  ASSERT_TRUE(translate_prefs->IsBlockedLanguage(kLanguage1));
  translate_prefs->AddLanguagePairToAlwaysTranslateList(kLanguage1, kLanguage2);
  ASSERT_TRUE(translate_prefs->IsLanguagePairOnAlwaysTranslateList(kLanguage1,
                                                                   kLanguage2));
  // Reset the preferences through the UI.
  CreateController();
  TranslateTableViewController* controller =
      static_cast<TranslateTableViewController*>(this->controller());
  // Simulate a tap on the "reset" item.
  [controller tableView:controller.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]];
  // Check that preferences are gone.
  EXPECT_FALSE(translate_prefs->IsSiteOnNeverPromptList(kBlockedSite));
  EXPECT_FALSE(translate_prefs->IsBlockedLanguage(kLanguage1));
  EXPECT_FALSE(translate_prefs->IsLanguagePairOnAlwaysTranslateList(
      kLanguage1, kLanguage2));
}

}  // namespace
