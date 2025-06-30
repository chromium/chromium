// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"

@implementation BWGSessionHandler {
  // The pref service to store the session data.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - BWGSessionDelegate

- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID {
  // TODO(crbug.com/419070203): Implement.
}

- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID {
  // TODO(crbug.com/419070203): Implement.
}

- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID {
  // TODO(crbug.com/419070203): Implement.
}

- (void)didTapBWGSettingsButton {
  // TODO(crbug.com/419070203): Implement.
}

@end
