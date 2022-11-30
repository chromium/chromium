// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_banner_interaction_handler.h"

#import "components/translate/core/browser/mock_translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TranslateInfobarBannerInteractionHandler.
class TranslateInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  TranslateInfobarBannerInteractionHandlerTest()
      : handler_(), delegate_factory_("fr", "en") {}

  translate::testing::MockTranslateInfoBarDelegate& GetMockDelegate(
      InfoBarIOS* infobar) {
    return *static_cast<translate::testing::MockTranslateInfoBarDelegate*>(
        infobar->delegate());
  }

 protected:
  TranslateInfobarBannerInteractionHandler handler_;
  translate::testing::MockTranslateInfoBarDelegateFactory delegate_factory_;
};

// Tests MainButtonTapped() calls Translate() on the mock delegate.
TEST_F(TranslateInfobarBannerInteractionHandlerTest, MainButton) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE));
  EXPECT_CALL(GetMockDelegate(infobar.get()), Translate());
  handler_.MainButtonTapped(infobar.get());
}

// Tests MainButtonTapped() calls RevertWithoutClosingInfobar() on the mock
// delegate.
TEST_F(TranslateInfobarBannerInteractionHandlerTest, MainButtonRevert) {
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateMockTranslateInfoBarDelegate(
          translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE));
  EXPECT_CALL(GetMockDelegate(infobar.get()), RevertWithoutClosingInfobar());
  handler_.MainButtonTapped(infobar.get());
}
