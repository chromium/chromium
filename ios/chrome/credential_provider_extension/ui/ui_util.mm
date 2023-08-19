// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/ui_util.h"

#import <AuthenticationServices/AuthenticationServices.h>

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
