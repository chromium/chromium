// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mutator.h"

@protocol IncognitoLockConsumer;
class PrefService;

// Mediator for the Incognito lock settings page UI.
@interface IncognitoLockMediator : NSObject <IncognitoLockMutator>

// Consumer for mediator.
@property(nonatomic, weak) id<IncognitoLockConsumer> consumer;

// Designated initializer. All the parameters should not be null.
// `localState`: local state pref service.
- (instancetype)initWithLocalState:(PrefService*)localState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cleans up anything before mediator shuts down.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_INCOGNITO_INCOGNITO_LOCK_MEDIATOR_H_
