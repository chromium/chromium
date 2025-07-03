// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

@implementation BWGSessionHandler {
  // The associated WebStateList.
  raw_ptr<WebStateList> _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
  }
  return self;
}

#pragma mark - BWGSessionDelegate

- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
}

- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
  [self setSessionActive:YES clientID:clientID];
}

- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID {
  [self setSessionActive:NO clientID:clientID];
}

- (void)didTapBWGSettingsButton {
  // TODO(crbug.com/419070203): Implement.
}

#pragma mark - Private

// Finds the web state with the given client ID as unique identifier.
- (web::WebState*)webStateWithClientID:(NSString*)clientID {
  for (int i = 0; i < _webStateList->count(); i++) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    NSString* webStateUniqueID = base::SysUTF8ToNSString(
        base::NumberToString(webState->GetUniqueIdentifier().identifier()));
    if ([webStateUniqueID isEqualToString:clientID]) {
      return webState;
    }
  }

  return nil;
}

// Updates the session state in storage with the given client ID and server ID.
- (void)updateSessionWithClientID:(NSString*)clientID
                         serverID:(NSString*)serverID {
  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->CreateOrUpdateBwgSessionInStorage(
      base::SysNSStringToUTF8(serverID));
}

// Sets the session's active state for the given client ID.
- (void)setSessionActive:(BOOL)active clientID:(NSString*)clientID {
  web::WebState* webState = [self webStateWithClientID:clientID];
  if (!webState) {
    return;
  }

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->SetBwgUiShowing(active);
}

@end
