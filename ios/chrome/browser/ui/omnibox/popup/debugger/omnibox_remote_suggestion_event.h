// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_H_

#import "base/unguessable_token.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

// A class that captures the state of a RemoteSuggestionsService event
@interface OmniboxRemoteSuggestionEvent : NSObject <OmniboxEvent>

// Contains the request URL as a string.
@property(nonatomic, strong) NSString* requestURL;
// Contains the request body of the remote suggestion service.
@property(nonatomic, strong) NSString* requestBody;
// Contains the response body of the remote suggestion service.
@property(nonatomic, strong) NSString* responseBody;
// Contains the response code of the remote suggestion service.
@property(nonatomic, assign) NSInteger responseCode;

- (instancetype)initWithUniqueIdentifier:
    (const base::UnguessableToken&)requestIdentifier;

// Returns the uniqueIdentifier of the event suggestion service.
- (const base::UnguessableToken&)uniqueIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_H_
