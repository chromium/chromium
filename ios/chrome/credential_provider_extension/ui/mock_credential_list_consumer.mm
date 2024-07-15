// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/mock_credential_list_consumer.h"

@implementation MockCredentialListConsumer

@synthesize delegate = _delegate;

- (void)presentSuggestedCredentials:(NSArray<id<Credential>>*)suggested
                     allCredentials:(NSArray<id<Credential>>*)all
                      showSearchBar:(BOOL)showSearchBar
              showNewPasswordOption:(BOOL)showNewPasswordOption {
  if (self.presentSuggestedCredentialsBlock) {
    self.presentSuggestedCredentialsBlock(suggested, all, showSearchBar,
                                          showNewPasswordOption);
  }
}

- (void)setTopPrompt:(NSString*)prompt {
}

@end
