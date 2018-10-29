// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/voicesearch_collection_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
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

class VoicesearchCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  VoicesearchCollectionViewControllerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
  }

  CollectionViewController* InstantiateController() override {
    return [[VoicesearchCollectionViewController alloc]
        initWithPrefs:pref_service_.get()];
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    TestingPrefServiceSimple* prefs = new TestingPrefServiceSimple();
    PrefRegistrySimple* registry = prefs->registry();
    registry->RegisterBooleanPref(prefs::kVoiceSearchTTS, false);
    registry->RegisterStringPref(prefs::kVoiceSearchLocale, "en-US");
    return std::unique_ptr<PrefService>(prefs);
  }

  LegacySettingsSwitchCell* GetSwitchCell() {
    return base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(
        [controller().collectionView
            cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0
                                                       inSection:0]]);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<PrefService> pref_service_;
};

TEST_F(VoicesearchCollectionViewControllerTest, NumberOfSectionsAndItems) {
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
TEST_F(VoicesearchCollectionViewControllerTest, TapTheLastItemInTheList) {
  CreateController();

  const std::vector<voice::SpeechInputLocale>& locales =
      voice::SpeechInputLocaleConfig::GetInstance()->GetAvailableLocales();
  // Add one to the available locale list size to account for the default locale
  // preference.
  ASSERT_EQ(locales.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(1)));

  // Simulate a tap on the last item in the list.
  [controller() collectionView:[controller() collectionView]
      didSelectItemAtIndexPath:[NSIndexPath indexPathForItem:locales.size()
                                                   inSection:1]];
}

TEST_F(VoicesearchCollectionViewControllerTest,
       TestModel_TextToSpeechOff_TTSSupported) {
  CreateController();
  LegacySettingsSwitchItem* switchItem = GetCollectionViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);
}

TEST_F(VoicesearchCollectionViewControllerTest,
       TestModel_TextToSpeechOn_TTSSupported) {
  // Enable the global TTS setting.
  BooleanPrefMember textToSpeechEnabled;
  textToSpeechEnabled.Init(prefs::kVoiceSearchTTS, pref_service_.get());
  textToSpeechEnabled.SetValue(true);

  CreateController();
  LegacySettingsSwitchItem* switchItem = GetCollectionViewItem(0, 0);
  EXPECT_TRUE(switchItem.isOn);
  EXPECT_TRUE(switchItem.isEnabled);
}

TEST_F(VoicesearchCollectionViewControllerTest,
       TestModel_TextToSpeechOff_TTSNotSupported) {
  // Set current language to a language that doesn't support TTS.
  StringPrefMember selectedLanguage;
  selectedLanguage.Init(prefs::kVoiceSearchLocale, pref_service_.get());
  selectedLanguage.SetValue("af-ZA");

  CreateController();
  LegacySettingsSwitchItem* switchItem = GetCollectionViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

TEST_F(VoicesearchCollectionViewControllerTest,
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
  LegacySettingsSwitchItem* switchItem = GetCollectionViewItem(0, 0);
  EXPECT_FALSE(switchItem.isOn);
  EXPECT_FALSE(switchItem.isEnabled);
}

}  // namespace
