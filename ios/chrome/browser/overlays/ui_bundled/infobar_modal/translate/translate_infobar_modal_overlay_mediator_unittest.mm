// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/translate/translate_infobar_modal_overlay_mediator.h"

#import "base/apple/bundle_locations.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/translate/core/browser/translate_step.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/translate/model/fake_translate_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"
#import "ios/chrome/browser/ui/infobars/modals/test/fake_infobar_translate_modal_consumer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::SysUTF16ToNSString;

// Base test fixture for TranslateInfobarModalOverlayMediator. The state of the
// mediator's consumer is expected to vary based on the current TranslateStep.
// Derived test fixtures are used to verify this behaviour.
class TranslateInfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  TranslateInfobarModalOverlayMediatorTest(
      translate::TranslateStep step,
      translate::TranslateErrors error_type)
      : delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))) {
    std::unique_ptr<FakeTranslateInfoBarDelegate> delegate =
        delegate_factory_.CreateFakeTranslateInfoBarDelegate("fr", "en", step,
                                                             error_type);
    translate_delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeTranslate,
                                            std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    mediator_ = [[TranslateInfobarModalOverlayMediator alloc]
        initWithRequest:request_.get()];
    mediator_.delegate = delegate_;
  }

  // Default constructor using TRANSLATE_STEP_BEFORE_TRANSLATE as the
  // translate step.
  TranslateInfobarModalOverlayMediatorTest()
      : TranslateInfobarModalOverlayMediatorTest(
            translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
            translate::TranslateErrors::NONE) {}

  ~TranslateInfobarModalOverlayMediatorTest() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

  FakeTranslateInfoBarDelegate& delegate() {
    return *static_cast<FakeTranslateInfoBarDelegate*>(infobar_->delegate());
  }

 protected:
  raw_ptr<FakeTranslateInfoBarDelegate> translate_delegate_;
  FakeTranslateInfoBarDelegateFactory delegate_factory_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  TranslateInfobarModalOverlayMediator* mediator_ = nil;
};

// Tests that a TranslateInfobarModalOverlayMediator correctly sets up its
// consumer while in a "before translate" step.
TEST_F(TranslateInfobarModalOverlayMediatorTest, SetUpConsumer) {
  FakeInfobarTranslateModalConsumer* consumer =
      [[FakeInfobarTranslateModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().source_language_name()),
              consumer.sourceLanguage);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().target_language_name()),
              consumer.targetLanguage);
  EXPECT_TRUE(consumer.enableTranslateActionButton);
  EXPECT_FALSE(consumer.updateLanguageBeforeTranslate);
  EXPECT_FALSE(consumer.displayShowOriginalButton);
  EXPECT_FALSE(consumer.shouldAlwaysTranslate);
  EXPECT_TRUE(consumer.shouldDisplayNeverTranslateLanguageButton);
  EXPECT_TRUE(consumer.isTranslatableLanguage);
  EXPECT_TRUE(consumer.shouldDisplayNeverTranslateSiteButton);
  EXPECT_FALSE(consumer.isSiteOnNeverPromptList);
}

// Tests that TranslateInfobarModalOverlayMediator calls RevertTranslation when
// its showSourceLanguage API is called.
TEST_F(TranslateInfobarModalOverlayMediatorTest, ShowSourceLanguage) {
  // TODO(crbug.com/40280062): Change translate_delegate_ to a mock
  // object, and verify that RevertWithoutClosingInfobar() is called.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ showSourceLanguage];
}

// Tests that TranslateInfobarModalOverlayMediator calls UpdateLanguageInfo and
// InfobarModalMainActionResponse when its translateWithNewLanguages API is
// called, and didSelectSourceLanguageIndex and didSelectTargetLanguageIndex
// were called beforehand to change the source and target languages.
TEST_F(TranslateInfobarModalOverlayMediatorTest, UpdateLanguageInfo) {
  // The order of the languages depends on the locale the device is running.
  // Skip the test if the locale is no en-US where the indexes have been
  // computed.
  NSString* currentLanguage =
      [[base::apple::FrameworkBundle() preferredLocalizations] firstObject];
  if (![currentLanguage isEqualToString:@"en-US"]) {
    return;
  }

  const int portuguese_index = 67;
  const int spanish_index = 81;

  const std::u16string kSourceLanguage = u"Portuguese";
  const std::u16string kTargetLanguage = u"Spanish";

  [mediator_ didSelectSourceLanguageIndex:portuguese_index
                                 withName:SysUTF16ToNSString(kSourceLanguage)];
  [mediator_ didSelectTargetLanguageIndex:spanish_index
                                 withName:SysUTF16ToNSString(kTargetLanguage)];

  EXPECT_EQ(kSourceLanguage, translate_delegate_->source_language_name());
  EXPECT_EQ(kTargetLanguage, translate_delegate_->target_language_name());
  // TODO(crbug.com/40280062): Change translate_delegate_ to a mock
  // object, and verify that Translate is called.

  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ translateWithNewLanguages];
}

// Tests that TranslateInfobarModalOverlayMediator calls ToggleAlwaysTranslate
// and InfobarModalMainActionResponse when its alwaysTranslateSourceLanguage API
// is called.
TEST_F(TranslateInfobarModalOverlayMediatorTest,
       AlwaysTranslateSourceLanguage) {
  // TODO(crbug.com/40280062): Change translate_delegate_ to a mock
  // object, and verify that ToggleAlwaysTranslate and Translate are called.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ alwaysTranslateSourceLanguage];
}

// Tests that TranslateInfobarModalOverlayMediator calls
// ToggleNeverTranslateSourceLanguage when its neverTranslateSourceLanguage API
// is called.
TEST_F(TranslateInfobarModalOverlayMediatorTest, NeverTranslateSourceLanguage) {
  // TODO(crbug.com/40280062): Change translate_delegate_ to a mock
  // object, and verify that ToggleTranslatableLanguageByPrefs is called.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ neverTranslateSourceLanguage];
}

// Tests that TranslateInfobarModalOverlayMediator calls ToggleNeverPromptSite
// when its neverTranslateSite API is called.
TEST_F(TranslateInfobarModalOverlayMediatorTest, NeverTranslateSite) {
  // TODO(crbug.com/40280062): Change translate_delegate_ to a mock
  // object, and verify that ToggleNeverPromptSite is called.
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ neverTranslateSite];
}

// Test fixture for TranslateInfobarModalOverlayMediator using the
// TRANSLATE_STEP_AFTER_TRANSLATE translate step.
class TranslateInfobarModalOverlayMediatorAfterTranslateTest
    : public TranslateInfobarModalOverlayMediatorTest {
 public:
  TranslateInfobarModalOverlayMediatorAfterTranslateTest()
      : TranslateInfobarModalOverlayMediatorTest(
            translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE,
            translate::TranslateErrors::NONE) {}
};

// Tests that a TranslateInfobarModalOverlayMediator correctly sets up its
// consumer while in an "after translate" step.
TEST_F(TranslateInfobarModalOverlayMediatorAfterTranslateTest, SetUpConsumer) {
  FakeInfobarTranslateModalConsumer* consumer =
      [[FakeInfobarTranslateModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().source_language_name()),
              consumer.sourceLanguage);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().target_language_name()),
              consumer.targetLanguage);
  EXPECT_FALSE(consumer.enableTranslateActionButton);
  EXPECT_FALSE(consumer.updateLanguageBeforeTranslate);
  EXPECT_TRUE(consumer.displayShowOriginalButton);
  EXPECT_FALSE(consumer.shouldAlwaysTranslate);
  EXPECT_FALSE(consumer.shouldDisplayNeverTranslateLanguageButton);
  EXPECT_TRUE(consumer.isTranslatableLanguage);
  EXPECT_FALSE(consumer.shouldDisplayNeverTranslateSiteButton);
  EXPECT_FALSE(consumer.isSiteOnNeverPromptList);
}

// Test fixture for TranslateInfobarModalOverlayMediator using the
// TRANSLATE_STEP_TRANSLATE_ERROR translate step.
class TranslateInfobarModalOverlayMediatorTranslateErrorTest
    : public TranslateInfobarModalOverlayMediatorTest {
 public:
  TranslateInfobarModalOverlayMediatorTranslateErrorTest()
      : TranslateInfobarModalOverlayMediatorTest(
            translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR,
            translate::TranslateErrors::TRANSLATION_ERROR) {}
};

// Tests that a TranslateInfobarModalOverlayMediator correctly sets up its
// consumer while in a "translate error" step. This is expected to behave the
// same as in the "before translate" step.
TEST_F(TranslateInfobarModalOverlayMediatorTranslateErrorTest, SetUpConsumer) {
  FakeInfobarTranslateModalConsumer* consumer =
      [[FakeInfobarTranslateModalConsumer alloc] init];
  mediator_.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().source_language_name()),
              consumer.sourceLanguage);
  EXPECT_NSEQ(base::SysUTF16ToNSString(delegate().target_language_name()),
              consumer.targetLanguage);
  EXPECT_TRUE(consumer.enableTranslateActionButton);
  EXPECT_FALSE(consumer.updateLanguageBeforeTranslate);
  EXPECT_FALSE(consumer.displayShowOriginalButton);
  EXPECT_FALSE(consumer.shouldAlwaysTranslate);
  EXPECT_TRUE(consumer.shouldDisplayNeverTranslateLanguageButton);
  EXPECT_TRUE(consumer.isTranslatableLanguage);
  EXPECT_TRUE(consumer.shouldDisplayNeverTranslateSiteButton);
  EXPECT_FALSE(consumer.isSiteOnNeverPromptList);
}
