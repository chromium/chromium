// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <string>

#import "components/webauthn/ios/ios_passkey_client.h"
#import "url/gurl.h"

class WebStateList;
@protocol PasskeyCreationBottomSheetConsumer;
@protocol PasskeyCreationBottomSheetMediatorDelegate;
@protocol ReauthenticationProtocol;

// Mediator for the passkey creation bottom sheet.
@interface PasskeyCreationBottomSheetMediator : NSObject

- (instancetype)
    initWithWebStateList:(WebStateList*)webStateList
             requestInfo:(webauthn::IOSPasskeyClient::RequestInfo)requestInfo
        accountForSaving:(NSString*)accountForSaving
            reauthModule:(id<ReauthenticationProtocol>)reauthModule
                delegate:(id<PasskeyCreationBottomSheetMediatorDelegate>)
                             mediatorDelegate;

// The passkey creation bottom sheet consumer.
@property(nonatomic, weak) id<PasskeyCreationBottomSheetConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

// Requests that a passkey be created.
- (void)createPasskey;

// Cancels the passkey request and defers to the renderer to save the passkey,
// potentially using a different credential provider.
- (void)deferPasskeyCreationToRenderer;

// Returns whether this mediator is currently fulfilling the given request.
- (BOOL)hasPendingRequest:
    (const webauthn::IOSPasskeyClient::RequestInfo&)requestInfo;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_H_
