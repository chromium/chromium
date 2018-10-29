// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/translate_table_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using user_prefs::PrefRegistrySyncable;

namespace {

const char kBlacklistedSite[] = "http://blacklistedsite.com";
const char kLanguage1[] = "klingon";
const char kLanguage2[] = "pirate";

class TranslateTableViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  TranslateTableViewControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[TranslateTableViewController alloc]
        initWithPrefs:pref_service_.get()];
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    scoped_refptr<PrefRegistrySyncable> registry = new PrefRegistrySyncable();
    registry->RegisterBooleanPref(prefs::kOfferTranslateEnabled, false,
                                  PrefRegistrySyncable::SYNCABLE_PREF);
    translate::TranslatePrefs::RegisterProfilePrefs(registry.get());
    registry->RegisterStringPref(
        prefs::kAcceptLanguages,
        l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
    base::FilePath path("TranslateTableViewControllerTest.pref");
    sync_preferences::PrefServiceMockFactory factory;
    factory.SetUserPrefsFile(path, base::ThreadTaskRunnerHandle::Get().get());
    return factory.Create(registry.get());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
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
  translateEnabled.Init(prefs::kOfferTranslateEnabled, pref_service_.get());
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
  translate_prefs->BlacklistSite(kBlacklistedSite);
  ASSERT_TRUE(translate_prefs->IsSiteBlacklisted(kBlacklistedSite));
  translate_prefs->AddToLanguageList(kLanguage1, /*force_blocked=*/true);
  ASSERT_TRUE(translate_prefs->IsBlockedLanguage(kLanguage1));
  translate_prefs->WhitelistLanguagePair(kLanguage1, kLanguage2);
  ASSERT_TRUE(
      translate_prefs->IsLanguagePairWhitelisted(kLanguage1, kLanguage2));
  // Reset the preferences through the UI.
  CreateController();
  TranslateTableViewController* controller =
      static_cast<TranslateTableViewController*>(this->controller());
  // Simulate a tap on the "reset" item.
  [controller tableView:controller.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]];
  // Check that preferences are gone.
  EXPECT_FALSE(translate_prefs->IsSiteBlacklisted(kBlacklistedSite));
  EXPECT_FALSE(translate_prefs->IsBlockedLanguage(kLanguage1));
  EXPECT_FALSE(
      translate_prefs->IsLanguagePairWhitelisted(kLanguage1, kLanguage2));
}

}  // namespace
