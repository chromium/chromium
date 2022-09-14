// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"

#import "components/translate/core/browser/mock_translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TranslateInfobarModalInteractionHandler.
class TranslateInfobarModalInteractionHandlerTest : public PlatformTest {
 public:
  TranslateInfobarModalInteractionHandlerTest()
      : handler_(), delegate_factory_("fr", "en") {}

  translate::testing::MockTranslateInfoBarDelegate& mock_delegate(
      InfoBarIOS* infobar) {
    return *static_cast<translate::testing::MockTranslateInfoBarDelegate*>(
        infobar->delegate());
  }

 protected:
  TranslateInfobarModalInteractionHandler handler_;
  translate::testing::MockTranslateInfoBarDelegateFactory delegate_factory_;
};

TEST_F(TranslateInfobarModalInteractionHandlerTest, MainButton) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(mock_delegate(infobar.get()), Translate());
  handler_.PerformMainAction(infobar.get());
}

TEST_F(TranslateInfobarModalInteractionHandlerTest, ToggleAlwaysTranslate) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(mock_delegate(infobar.get()), Translate());
  EXPECT_CALL(mock_delegate(infobar.get()), ToggleAlwaysTranslate());
  handler_.ToggleAlwaysTranslate(infobar.get());
}

TEST_F(TranslateInfobarModalInteractionHandlerTest,
       ToggleNeverTranslateLanguage) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(mock_delegate(infobar.get()),
              ToggleTranslatableLanguageByPrefs());
  handler_.ToggleNeverTranslateLanguage(infobar.get());
}

TEST_F(TranslateInfobarModalInteractionHandlerTest, ToggleNeverTranslateSite) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(mock_delegate(infobar.get()), ToggleNeverPromptSite());
  handler_.ToggleNeverTranslateSite(infobar.get());
}

TEST_F(TranslateInfobarModalInteractionHandlerTest, RevertTranslation) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(mock_delegate(infobar.get()), RevertWithoutClosingInfobar());
  handler_.RevertTranslation(infobar.get());
}

TEST_F(TranslateInfobarModalInteractionHandlerTest, UpdateLanguages) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  int source_index = 0;
  int target_index = 1;
  // Just assert that the methods are called. The actual codes are unecessary to
  // mock since it is dependent on the Translate model.
  EXPECT_CALL(mock_delegate(infobar.get()), UpdateTargetLanguage(_));
  EXPECT_CALL(mock_delegate(infobar.get()), UpdateSourceLanguage(_));
  handler_.UpdateLanguages(infobar.get(), source_index, target_index);
}
