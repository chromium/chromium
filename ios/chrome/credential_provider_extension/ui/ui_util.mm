// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

#import <AuthenticationServices/AuthenticationServices.h>

namespace {

// System SF symbol names for the hide and show actions.
NSString* const kHideActionSymbol = @"eye.slash";
NSString* const kShowActionSymbol = @"eye";

// Size of the action icons.
const CGFloat kSymbolActionPointSize = 15;

// System SF symbol name for the info button icon.
NSString* const kInfoCircleSymbol = @"info.circle";

// Size of the info button icon.
const CGFloat kInfoSymbolPointSize = 20;

// System SF symbol name for the error icon.
NSString* const kErrorCircleFillSymbol = @"exclamationmark.circle.fill";

// Size of the error icon.
const CGFloat kErrorSymbolPointSize = 20;

UIImage* DefaultSymbolWithPointSize(NSString* symbol_name, CGFloat point_size) {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:point_size
                          weight:UIImageSymbolWeightMedium
                           scale:UIImageSymbolScaleMedium];
  return [[UIImage systemImageNamed:symbol_name withConfiguration:configuration]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

}  // namespace

const CGFloat kUITableViewInsetGroupedTopSpace = 35;

NSString* HostForServiceIdentifier(
    ASCredentialServiceIdentifier* serviceIdentifier) {
  NSString* identifier = serviceIdentifier.identifier;
  NSURL* promptURL = identifier ? [NSURL URLWithString:identifier] : nil;
  return promptURL.host ?: identifier;
}

NSString* PromptForServiceIdentifiers(
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentifiers) {
  NSString* IDForPrompt =
      HostForServiceIdentifier(serviceIdentifiers.firstObject);
  if (!IDForPrompt) {
    return nil;
  }
  NSString* baseLocalizedString = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_TITLE_PROMPT",
      @"Extra prompt telling the user what site they are looking at");
  return [baseLocalizedString stringByReplacingOccurrencesOfString:@"$1"
                                                        withString:IDForPrompt];
}

UIImage* GetPasswordVisibilityIcon(bool is_visible) {
  NSString* symbol_name = is_visible ? kHideActionSymbol : kShowActionSymbol;
  return DefaultSymbolWithPointSize(symbol_name, kSymbolActionPointSize);
}

UIImage* GetCredentialInfoIcon() {
  return DefaultSymbolWithPointSize(kInfoCircleSymbol, kInfoSymbolPointSize);
}

UIImage* GetNoteErrorIcon() {
  return DefaultSymbolWithPointSize(kErrorCircleFillSymbol,
                                    kErrorSymbolPointSize);
}
