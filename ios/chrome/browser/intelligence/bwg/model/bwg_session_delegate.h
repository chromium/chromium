// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Input type for BWG queries.
typedef NS_ENUM(NSInteger, BWGInputType) {
  // Unknown input type.
  BWGInputTypeUnknown = 0,
  // Text input type.
  BWGInputTypeText = 1,
  // Summarize input type.
  BWGInputTypeSummarize = 2,
  // Check this site input type.
  BWGInputTypeCheckThisSite = 3,
  // Find related sites input type.
  BWGInputTypeFindRelatedSites = 4,
  // Ask about page input type.
  BWGInputTypeAskAboutPage = 5,
  // Create FAQ input type.
  BWGInputTypeCreateFaq = 6,
  // Zero state model suggestion input type.
  BWGInputTypeZeroStateModelSuggestion = 7,
  // 'What can Gemini do' input type.
  BWGInputTypeWhatCanGeminiDo = 8,
  // Discovery card input type.
  BWGInputTypeDiscoveryCard = 9,
};

// Delegate for BWG session events. Keep up to date with GCR's SessionDelegate.
@protocol BWGSessionDelegate

// Called when a new session is created.
- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID;

// Called when the UI is shown.
- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID;

// Called when the UI is hidden.
- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID;

// Called when a response is received.
- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID;

// Called when the user taps the BWG settings button from within the BWG UI.
- (void)didTapBWGSettingsButton;

// Called when a query is sent with the specified input type and context info
// and whether the page context was attached
- (void)didSendQueryWithInputType:(BWGInputType)inputType
              pageContextAttached:(BOOL)pageContextAttached;

// Called when a new chat button is tapped.
// TODO(crbug.com/436019705) Rename this to `clientID` and `serverID`.
- (void)didTapNewChatButtonWithSessionID:(NSString*)sessionID
                          conversationID:(NSString*)conversationID;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
