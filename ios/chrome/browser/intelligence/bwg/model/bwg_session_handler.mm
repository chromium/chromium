// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@implementation BWGSessionHandler {
  // The associated WebStateList.
  raw_ptr<WebStateList> _webStateList;
  // Session start time for duration tracking.
  base::TimeTicks _sessionStartTime;
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
  // Start session timer.
  _sessionStartTime = base::TimeTicks::Now();
}

- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID {
  [_BWGHandler dismissBWGFlowWithCompletion:nil];
  [self setSessionActive:NO clientID:clientID];
  // Record session duration.
  if (!_sessionStartTime.is_null()) {
    base::TimeDelta session_duration =
        base::TimeTicks::Now() - _sessionStartTime;
    RecordBWGSessionTime(session_duration);
    _sessionStartTime = base::TimeTicks();
  }
}

- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID {
  [self updateSessionWithClientID:clientID serverID:serverID];
}

- (void)didTapBWGSettingsButton {
  [self.settingsHandler showBWGSettings];
}

- (void)didSendQueryWithInputType:(BWGInputType)inputType
              pageContextAttached:(BOOL)pageContextAttached {
  // TODO(crbug.com/434758568): Add metrics logging for query sent events.
}

// Called when a new chat button is tapped.
- (void)didTapNewChatButtonWithSessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID {
  web::WebState* webState = [self webStateWithClientID:sessionID];
  if (!webState) {
    return;
  }
  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  BWGTabHelper->DeleteBwgSessionInStorage();
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
