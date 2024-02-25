// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_

#include <string>

#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}
namespace translate {
class TranslateManager;
namespace testing {
class MockTranslateClient;
class MockTranslateRanker;
class MockLanguageModel;
}  // namespace testing
}  // namespace translate

// Fake of TranslateInfoBarDelegate that allows for triggering Observer
// callbacks.
class FakeTranslateInfoBarDelegate
    : public translate::TranslateInfoBarDelegate {
 public:
  FakeTranslateInfoBarDelegate(
      const base::WeakPtr<translate::TranslateManager>& translate_manager,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool triggered_from_menu);
  ~FakeTranslateInfoBarDelegate() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Call the OnTranslateStepChanged() observer method on all
  // `OnTranslateStepChanged`.
  void TriggerOnTranslateStepChanged(translate::TranslateStep step,
                                     translate::TranslateErrors error_type);

  std::u16string source_language_name() const override;

  std::u16string target_language_name() const override;

  bool IsTranslatableLanguageByPrefs() const override;

 private:
  base::ObserverList<Observer, true> observers_;
  std::u16string source_language_;
  std::u16string target_language_;
  bool translatable_language_ = true;
};

// Factory class to create instances of FakeTranslateInfoBarDelegate.
class FakeTranslateInfoBarDelegateFactory {
 public:
  FakeTranslateInfoBarDelegateFactory();
  ~FakeTranslateInfoBarDelegateFactory();

  // Create a FakeTranslateInfoBarDelegate unique_ptr with
  // `source_language`, `target_language`, `translate_step` and `error_type`.
  std::unique_ptr<FakeTranslateInfoBarDelegate>
  CreateFakeTranslateInfoBarDelegate(
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateStep translate_step =
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate::TranslateErrors error_type = translate::TranslateErrors::NONE);

 private:
  translate::testing::MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<translate::testing::MockTranslateClient> client_;
  std::unique_ptr<translate::testing::MockTranslateRanker> ranker_;
  std::unique_ptr<translate::testing::MockLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> manager_;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_
