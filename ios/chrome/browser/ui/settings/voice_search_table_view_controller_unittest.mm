// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/browser/voice/speech_input_locale_config_impl.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class VoiceSearchTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  VoiceSearchTableViewControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[VoiceSearchTableViewController alloc]
        initWithPrefs:pref_service_.get()];
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
    PrefRegistrySimple* registry = prefs->registry();
    registry->RegisterBooleanPref(prefs::kVoiceSearchTTS, false);
    registry->RegisterStringPref(prefs::kVoiceSearchLocale, "en-US");
    return std::unique_ptr<PrefService>(prefs);
  }

  SettingsSwitchCell* GetSwitchCell() {
    return base::mac::ObjCCastStrict<SettingsSwitchCell>([controller().tableView
        cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<PrefService> pref_service_;
};

TEST_F(VoiceSearchTableViewControllerTest, NumberOfSectionsAndItems) {
  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  const std::vector<voice::SpeechInputLocale>& locales =
      voice::SpeechInputLocaleConfig::GetInstance()->GetAvailableLocales();
  // Add one to the available locale list size to account for the default locale
  // preference.
  EXPECT_EQ(locales.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(1)));
}

// Taps the last item in the languages list to ensure that the app does not
// crash.  This tests for a regression in crbug.com/661358.
TEST_F(VoiceSearchTableViewControllerTest, TapTheLastItemInTheList) {
  CreateController();

  const std::vector<voice::SpeechInputLocale>& locales =
      voice::SpeechInputLocaleConfig::GetInstance()->GetAvailableLocales();
  // Add one to the available locale list size to account for the default locale
  // preference.
  ASSERT_EQ(locales.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // Simulate a tap on the last item in the list.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:locales.size()
                                                  inSection:1]];
}

TEST_F(VoiceSearchTableViewControllerTest,
       TestModel_TextToSpeechOff_TTSSupported) {
  CreateController();
  SettingsSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);
}

TEST_F(VoiceSearchTableViewControllerTest,
       TestModel_TextToSpeechOn_TTSSupported) {
  // Enable the global TTS setting.
  BooleanPrefMember textToSpeechEnabled;
  textToSpeechEnabled.Init(prefs::kVoiceSearchTTS, pref_service_.get());
  textToSpeechEnabled.SetValue(true);

  CreateController();
  SettingsSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_TRUE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);
}

TEST_F(VoiceSearchTableViewControllerTest,
       TestModel_TextToSpeechOff_TTSNotSupported) {
  // Set current language to a language that doesn't support TTS.
  StringPrefMember selectedLanguage;
  selectedLanguage.Init(prefs::kVoiceSearchLocale, pref_service_.get());
  selectedLanguage.SetValue("af-ZA");

  CreateController();
  SettingsSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

TEST_F(VoiceSearchTableViewControllerTest,
       TestModel_TextToSpeechOn_TTSNotSupported) {
  // Set current language to a language that doesn't support TTS.
  StringPrefMember selectedLanguage;
  selectedLanguage.Init(prefs::kVoiceSearchLocale, pref_service_.get());
  selectedLanguage.SetValue("af-ZA");

  // Enable the global TTS setting.
  BooleanPrefMember textToSpeechEnabled;
  textToSpeechEnabled.Init(prefs::kVoiceSearchTTS, pref_service_.get());
  textToSpeechEnabled.SetValue(true);

  CreateController();
  SettingsSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

}  // namespace
