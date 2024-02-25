// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/voice_search_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_config_impl.h"
#import "ios/chrome/browser/voice/model/voice_search_prefs.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class VoiceSearchTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  VoiceSearchTableViewControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
  }

  LegacyChromeTableViewController* InstantiateController() override {
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

  TableViewSwitchCell* GetSwitchCell() {
    return base::apple::ObjCCastStrict<TableViewSwitchCell>(
        [controller().tableView
            cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0
                                                      inSection:0]]);
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
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
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
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
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
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
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
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

// Verifies that the TTS switch item is updated when the underlying preference
// value changes.
TEST_F(VoiceSearchTableViewControllerTest, TTSPrefToggled) {
  // Enable the global TTS setting.
  BooleanPrefMember textToSpeechEnabled;
  textToSpeechEnabled.Init(prefs::kVoiceSearchTTS, pref_service_.get());
  textToSpeechEnabled.SetValue(true);

  CreateController();
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_TRUE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);

  // Disable the global TTS setting.
  textToSpeechEnabled.SetValue(false);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);

  // Re-enable the global TTS setting.
  textToSpeechEnabled.SetValue(true);
  EXPECT_TRUE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);
}

// Verifies that language items are updated when the underlying preference value
// changes.
TEST_F(VoiceSearchTableViewControllerTest, LanguagePrefChanged) {
  // Enable the global TTS setting and set the selected language as the
  // default language.
  BooleanPrefMember textToSpeechEnabled;
  textToSpeechEnabled.Init(prefs::kVoiceSearchTTS, pref_service_.get());
  textToSpeechEnabled.SetValue(true);
  StringPrefMember selectedLanguage;
  selectedLanguage.Init(prefs::kVoiceSearchLocale, pref_service_.get());
  selectedLanguage.SetValue("");

  CreateController();
  TableViewSwitchItem* switchItem = GetTableViewItem(0, 0);
  EXPECT_TRUE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);

  TableViewDetailTextItem* defaultLanguageItem = GetTableViewItem(1, 0);
  EXPECT_EQ(defaultLanguageItem.accessoryType,
            UITableViewCellAccessoryCheckmark);

  const std::vector<voice::SpeechInputLocale>& locales =
      voice::SpeechInputLocaleConfig::GetInstance()->GetAvailableLocales();

  // Add one to the available locale list size to account for the default locale
  // preference.
  ASSERT_EQ(locales.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(1)));

  TableViewDetailTextItem* lastLanguageItem =
      GetTableViewItem(1, locales.size());
  EXPECT_EQ(lastLanguageItem.accessoryType, UITableViewCellAccessoryNone);

  // Update the language preference to the last language.
  selectedLanguage.SetValue(locales[locales.size() - 1].code);
  EXPECT_EQ(defaultLanguageItem.accessoryType, UITableViewCellAccessoryNone);
  EXPECT_EQ(lastLanguageItem.accessoryType, UITableViewCellAccessoryCheckmark);

  // Update the language preference to a language that doesn't support TTS.
  selectedLanguage.SetValue("af-ZA");
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

}  // namespace
