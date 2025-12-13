// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_error_alert_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/credential_provider_extension/generated_localized_strings.h"

namespace {

// Custom size for the image when shown as a favicon. Used to override the base
// class' value.
constexpr CGFloat kCustomFaviconSideLength = 42;

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
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      return CredentialProviderPasskeyCreationEnterpriseDisabledTitleString();
    case ErrorType::kSignedOut:
      return CredentialProviderSignedOutUserTitleString();
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      return CredentialProviderPasskeyCreationUserDisabledTitleString();
  }
  return @"";
}

// Returns the subtitle to use depending on the provided `error_type`.
NSString* GetSubtitleString(ErrorType error_type) {
  switch (error_type) {
    case ErrorType::kEnterpriseDisabledSavingCredentials:
      return CredentialProviderPasskeyCreationEnterpriseDisabledSubtitleString();
    case ErrorType::kSignedOut:
      return CredentialProviderSignedOutUserSubtitleString();
    case ErrorType::kUserDisabledSavingCredentialsInPasswordSettings:
      return CredentialProviderPasskeyCreationUserDisabledInPasswordSettingsSubtitleString();
    case ErrorType::kUserDisabledSavingCredentialsToAccount:
      return CredentialProviderPasskeyCreationUserDisabledForAccountSubtitleString();
  }
  return @"";
}

}  // namespace

@implementation PasskeyErrorAlertViewController {
  // The error for which this view is shown. Used to appropriately set up the
  // UI elements.
  ErrorType _errorType;
}

- (instancetype)initForErrorType:(ErrorType)errorType {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString =
      CredentialProviderPasskeyErrorAlertButtonTitleString();
  self = [super initWithConfiguration:configuration];
  if (self) {
    _errorType = errorType;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = GetImage(_errorType);
  self.imageHasFixedSize = YES;
  if (ShouldShowImageAsFavicon(_errorType)) {
    self.customFaviconSideLength = kCustomFaviconSideLength;
    self.imageEnclosedWithShadowWithoutBadge = YES;
  }

  self.titleString = GetTitleString(_errorType);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString = GetSubtitleString(_errorType);
  [super viewDidLoad];
}

@end
