// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_error_alert_view_controller.h"

namespace {

// Custom size for the image when shown as a favicon. Used to override the base
// class' value.
constexpr CGFloat kCustomFaviconSideLength = 42;

// Custom bottom margin for the primary action button. Used to override the base
// class' value.
constexpr CGFloat kCustomPrimaryActionButtonBottomMargin = 24;

// Point size of the image.
constexpr CGFloat kImagePointSize = 60;

// Returns the image to show in the UI depending on the provided `error_type`.
UIImage* GetImage(ErrorType error_type) {
  NSString* image_name;
  BOOL is_multicolor_symbol = NO;
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      image_name = @"cpe_enterprise_icon";
      break;
    case ErrorType::kSignedOut:
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      image_name = @"multicolor_chrome";
      is_multicolor_symbol = YES;
      break;
  }

  UIImage* image =
      [UIImage imageNamed:image_name
                   inBundle:nil
          withConfiguration:
              [UIImageSymbolConfiguration
                  configurationWithPointSize:kImagePointSize
                                      weight:UIImageSymbolWeightMedium
                                       scale:UIImageSymbolScaleMedium]];

  return is_multicolor_symbol
             ? [image imageByApplyingSymbolConfiguration:
                          [UIImageSymbolConfiguration
                              configurationPreferringMulticolor]]
             : image;
}

// Returns whether the image should be shown in a rounded square container.
BOOL ShouldShowImageAsFavicon(ErrorType error_type) {
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      return NO;
    case ErrorType::kSignedOut:
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      return YES;
  }
}

// Returns the title to use depending on the provided `error_type`.
NSString* GetTitleString(ErrorType error_type) {
  NSString* string_id;
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_ENTERPRISE_"
                  @"DISABLED_TITLE";
      break;
    case ErrorType::kSignedOut:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_SIGNED_OUT_USER_TITLE";
      break;
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      string_id =
          @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_USER_DISABLED_TITLE";
      break;
  }
  return NSLocalizedString(string_id, @"The title of the screen.");
}

// Returns the subtitle to use depending on the provided `error_type`.
NSString* GetSubtitleString(ErrorType error_type) {
  NSString* string_id;
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_ENTERPRISE_"
                  @"DISABLED_SUBTITLE";
      break;
    case ErrorType::kSignedOut:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_SIGNED_OUT_USER_SUBTITLE";
      break;
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_USER_DISABLED_"
                  @"IN_PASSWORD_SETTINGS_SUBTITLE";
      break;
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      string_id = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_CREATION_USER_DISABLED_"
                  @"FOR_ACCOUNT_SUBTITLE";
      break;
  }
  return NSLocalizedString(string_id, @"The subtitle of the screen.");
}

}  // namespace

@implementation PasskeyErrorAlertViewController {
  // The error for which this view is shown. Used to appropriately set up the
  // UI elements.
  ErrorType _errorType;
}

- (instancetype)initForErrorType:(ErrorType)errorType {
  self = [super init];
  if (self) {
    _errorType = errorType;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.image = GetImage(_errorType);
  self.imageHasFixedSize = YES;
  if (ShouldShowImageAsFavicon(_errorType)) {
    self.customFaviconSideLength = kCustomFaviconSideLength;
    self.imageEnclosedWithShadowWithoutBadge = YES;
  }

  self.titleString = GetTitleString(_errorType);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString = GetSubtitleString(_errorType);
  self.primaryActionString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ERROR_ALERT_BUTTON_TITLE",
      @"The primary action title.");

  // Override the `actionStackBottomMargin` on iPhones as the default vertical
  // position of the primary button is too close to the bottom of the screen.
  // This issue isn't present on iPads, as the view is presented in the center
  // of the screen.
  if ([[UIDevice currentDevice] userInterfaceIdiom] ==
      UIUserInterfaceIdiomPhone) {
    self.actionStackBottomMargin = kCustomPrimaryActionButtonBottomMargin;
  }

  [super loadView];
}

@end
