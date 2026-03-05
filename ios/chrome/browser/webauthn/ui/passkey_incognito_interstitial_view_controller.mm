// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

NSString* const kPasskeyIncognitoInterstitialViewID =
    @"PasskeyIncognitoInterstitialViewID";

@implementation PasskeyIncognitoInterstitialViewController

- (void)loadView {
  // TODO(crbug.com/487898150): Add 'incognito' header image to the dialog box.
  self.titleString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_DIALOG_TITLE);
  self.subtitleString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_DIALOG_SUBTITLE);

  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_CONTINUE_BUTTON);
  self.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_CANCEL_BUTTON);

  [super loadView];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kPasskeyIncognitoInterstitialViewID;
  // TODO(crbug.com/487898150): Modify background color.
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.delegate passkeyIncognitoInterstitialViewDidDisappear];
}

@end
