// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class IncognitoReauthSceneAgent;
@protocol IncognitoReauthConsumer;

// Mediator handling incognito reauthentication. Uses the reauth scene agent as
// the source of truth for reauth state.
@interface IncognitoReauthMediator : NSObject

- (instancetype)initWithReauthAgent:(IncognitoReauthSceneAgent*)reauthAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<IncognitoReauthConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_MEDIATOR_H_
