// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_mediator.h"

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"

@interface IncognitoReauthMediator () <IncognitoReauthObserver>

// Agent tracking the authentication status.
@property(nonatomic, weak) IncognitoReauthSceneAgent* reauthAgent;

@end

@implementation IncognitoReauthMediator

- (instancetype)initWithReauthAgent:(IncognitoReauthSceneAgent*)reauthAgent {
  self = [super init];
  if (self) {
    _reauthAgent = reauthAgent;
    [reauthAgent addObserver:self];
  }
  return self;
}

- (void)setConsumer:(id<IncognitoReauthConsumer>)consumer {
  _consumer = consumer;
  [_consumer setItemsRequireAuthentication:_reauthAgent.authenticationRequired];
}

- (void)dealloc {
  [_reauthAgent removeObserver:self];
}

#pragma mark - IncognitoReauthObserver

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateAuthenticationRequirement:(BOOL)isRequired {
  [self.consumer setItemsRequireAuthentication:isRequired];
}

@end
