// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_mediator.h"

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoReauthMediator () <IncognitoReauthObserver>

// Consumer for this mediator.
@property(nonatomic, weak) id<IncognitoReauthConsumer> consumer;

// Agent tracking the authentication status.
@property(nonatomic, weak) IncognitoReauthSceneAgent* reauthAgent;

@end

@implementation IncognitoReauthMediator

- (instancetype)initWithConsumer:(id<IncognitoReauthConsumer>)consumer
                     reauthAgent:(IncognitoReauthSceneAgent*)reauthAgent {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _reauthAgent = reauthAgent;
    [reauthAgent addObserver:self];

    [_consumer
        setItemsRequireAuthentication:reauthAgent.authenticationRequired];
  }
  return self;
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
