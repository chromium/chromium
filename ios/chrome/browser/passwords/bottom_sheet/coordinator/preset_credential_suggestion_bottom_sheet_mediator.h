// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PRESET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PRESET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator_base.h"

class FaviconLoader;
class WebStateList;
@class FormSuggestion;

// Mediator responsible for displaying a preset list of credential form
// suggestions in the Credential Suggestion Bottom Sheet.
@interface PresetCredentialSuggestionBottomSheetMediator
    : CredentialSuggestionBottomSheetMediatorBase

// Designated initializer for this mediator. `webStateList` is the list of web
// states to observe. `faviconLoader` loads favicons for suggestions.
// `suggestions` is the preset list of form suggestions to display.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       faviconLoader:(FaviconLoader*)faviconLoader
                         suggestions:(NSArray<FormSuggestion*>*)suggestions
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
            reauthModule:(id<ReauthenticationProtocol>)reauthModule
             requestInfo:
                 (std::optional<webauthn::IOSPasskeyClient::RequestInfo>)
                     requestInfo NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PRESET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_MEDIATOR_H_
