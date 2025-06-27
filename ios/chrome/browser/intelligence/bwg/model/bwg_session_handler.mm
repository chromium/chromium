// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"

@implementation BWGSessionHandler

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
