// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/fake_translate_infobar_delegate.h"

#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/translate/core/browser/mock_translate_client.h"
#import "components/translate/core/browser/mock_translate_infobar_delegate.h"
#import "components/translate/core/browser/mock_translate_ranker.h"

using translate::testing::MockLanguageModel;
using translate::testing::MockTranslateClient;
using translate::testing::MockTranslateRanker;

FakeTranslateInfoBarDelegate::FakeTranslateInfoBarDelegate(
    const base::WeakPtr<translate::TranslateManager>& translate_manager,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool triggered_from_menu)
    : translate::TranslateInfoBarDelegate(translate_manager,
                                          step,
                                          source_language,
                                          target_language,
                                          error_type,
                                          triggered_from_menu),
      source_language_(source_language.begin(), source_language.end()),
      target_language_(target_language.begin(), target_language.end()) {}

FakeTranslateInfoBarDelegate::~FakeTranslateInfoBarDelegate() {
  for (auto& observer : observers_) {
    observer.OnTranslateInfoBarDelegateDestroyed(this);
  }
}

void FakeTranslateInfoBarDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeTranslateInfoBarDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeTranslateInfoBarDelegate::TriggerOnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors error_type) {
  for (auto& observer : observers_) {
    observer.OnTranslateStepChanged(step, error_type);
  }
}

std::u16string FakeTranslateInfoBarDelegate::source_language_name() const {
  return source_language_;
}

std::u16string FakeTranslateInfoBarDelegate::target_language_name() const {
  return target_language_;
}

bool FakeTranslateInfoBarDelegate::IsTranslatableLanguageByPrefs() const {
  return translatable_language_;
}

FakeTranslateInfoBarDelegateFactory::FakeTranslateInfoBarDelegateFactory() {
  pref_service_ =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
  translate::TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
  pref_service_->registry()->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true);
  client_ =
      std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
  ranker_ = std::make_unique<MockTranslateRanker>();
  language_model_ = std::make_unique<MockLanguageModel>();
  manager_ = std::make_unique<translate::TranslateManager>(
      client_.get(), ranker_.get(), language_model_.get());
}

FakeTranslateInfoBarDelegateFactory::~FakeTranslateInfoBarDelegateFactory() {}

std::unique_ptr<FakeTranslateInfoBarDelegate>
FakeTranslateInfoBarDelegateFactory::CreateFakeTranslateInfoBarDelegate(
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateStep translate_step,
    translate::TranslateErrors error_type) {
  return std::make_unique<FakeTranslateInfoBarDelegate>(
      manager_->GetWeakPtr(), translate_step, source_language, target_language,
      error_type, false);
}
