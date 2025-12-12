// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_error_alert_view_controller.h"

#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Helper method to get the desired image with the right configuration.
UIImage* GetImage(NSString* image_name, BOOL is_multicolor_symbol) {
  UIImage* image =
      [UIImage imageNamed:image_name
                   inBundle:nil
          withConfiguration:
              [UIImageSymbolConfiguration
                  configurationWithPointSize:60
                                      weight:UIImageSymbolWeightMedium
                                       scale:UIImageSymbolScaleMedium]];
  return is_multicolor_symbol
             ? [image imageByApplyingSymbolConfiguration:
                          [UIImageSymbolConfiguration
                              configurationPreferringMulticolor]]
             : image;
}

}  // namespace

class PasskeyErrorAlertViewControllerTest : public PlatformTest {
 public:
  // Creates a PasskeyErrorAlertViewController for the provided error type.
  PasskeyErrorAlertViewController* CreateController(ErrorType error_type) {
    return
        [[PasskeyErrorAlertViewController alloc] initForErrorType:error_type];
  }
};

// Tests that the view's content with the `kEnterpriseDisabledSavingCredentials`
// error type is as expected.
TEST_F(PasskeyErrorAlertViewControllerTest,
       TestContentWithEnterpriseErrorType) {
  PasskeyErrorAlertViewController* controller =
      CreateController(ErrorType::kEnterpriseDisabledSavingCredentials);
  [controller viewDidLoad];

  EXPECT_NSEQ(controller.image,
              GetImage(@"cpe_enterprise_icon", /*is_multicolor_symbol=*/NO));
  EXPECT_TRUE(controller.imageHasFixedSize);
  EXPECT_EQ(controller.customFaviconSideLength, 0);
  EXPECT_FALSE(controller.imageEnclosedWithShadowWithoutBadge);
  EXPECT_NSEQ(controller.titleString,
              CredentialProviderPasskeyCreationEnterpriseDisabledTitleString());
  EXPECT_NSEQ(
      controller.subtitleString,
      CredentialProviderPasskeyCreationEnterpriseDisabledSubtitleString());
  EXPECT_NSEQ(controller.primaryActionButton.titleLabel.text,
              CredentialProviderPasskeyErrorAlertButtonTitleString());
}

// Tests that the view's content with the `kSignedOut` error type is as
// expected.
TEST_F(PasskeyErrorAlertViewControllerTest, TestContentWithSignedOutErrorType) {
  PasskeyErrorAlertViewController* controller =
      CreateController(ErrorType::kSignedOut);
  [controller viewDidLoad];

  EXPECT_NSEQ(controller.image,
              GetImage(@"multicolor_chrome", /*is_multicolor_symbol=*/YES));
  EXPECT_TRUE(controller.imageHasFixedSize);
  EXPECT_EQ(controller.customFaviconSideLength, 42);
  EXPECT_TRUE(controller.imageEnclosedWithShadowWithoutBadge);
  EXPECT_NSEQ(controller.titleString,
              CredentialProviderSignedOutUserTitleString());
  EXPECT_NSEQ(controller.subtitleString,
              CredentialProviderSignedOutUserSubtitleString());
  EXPECT_NSEQ(controller.primaryActionButton.titleLabel.text,
              CredentialProviderPasskeyErrorAlertButtonTitleString());
}

// Tests that the view's content with the
// `kUserDisabledSavingCredentialsInPasswordSettings` error type is as expected.
TEST_F(PasskeyErrorAlertViewControllerTest,
       TestContentWithPasswordSettingsErrorType) {
  PasskeyErrorAlertViewController* controller = CreateController(
      ErrorType::kUserDisabledSavingCredentialsInPasswordSettings);
  [controller viewDidLoad];

  EXPECT_NSEQ(controller.image,
              GetImage(@"multicolor_chrome", /*is_multicolor_symbol=*/YES));
  EXPECT_TRUE(controller.imageHasFixedSize);
  EXPECT_EQ(controller.customFaviconSideLength, 42);
  EXPECT_TRUE(controller.imageEnclosedWithShadowWithoutBadge);
  EXPECT_NSEQ(controller.titleString,
              CredentialProviderPasskeyCreationUserDisabledTitleString());
  EXPECT_NSEQ(
      controller.subtitleString,
      CredentialProviderPasskeyCreationUserDisabledInPasswordSettingsSubtitleString());
  EXPECT_NSEQ(controller.primaryActionButton.titleLabel.text,
              CredentialProviderPasskeyErrorAlertButtonTitleString());
}

// Tests that the view's content with the
// `kUserDisabledSavingCredentialsToAccount` error type is as expected.
TEST_F(PasskeyErrorAlertViewControllerTest,
       TestContentWithAccountSettingsErrorType) {
  PasskeyErrorAlertViewController* controller =
      CreateController(ErrorType::kUserDisabledSavingCredentialsToAccount);
  [controller viewDidLoad];

  EXPECT_NSEQ(controller.image,
              GetImage(@"multicolor_chrome", /*is_multicolor_symbol=*/YES));
  EXPECT_TRUE(controller.imageHasFixedSize);
  EXPECT_EQ(controller.customFaviconSideLength, 42);
  EXPECT_TRUE(controller.imageEnclosedWithShadowWithoutBadge);
  EXPECT_NSEQ(controller.titleString,
              CredentialProviderPasskeyCreationUserDisabledTitleString());
  EXPECT_NSEQ(
      controller.subtitleString,
      CredentialProviderPasskeyCreationUserDisabledForAccountSubtitleString());
  EXPECT_NSEQ(controller.primaryActionButton.titleLabel.text,
              CredentialProviderPasskeyErrorAlertButtonTitleString());
}
