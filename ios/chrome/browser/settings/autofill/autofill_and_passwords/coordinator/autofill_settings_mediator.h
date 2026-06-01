// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AutofillSettingsConsumer;
class PrefService;

// Mediator for the Autofill settings page.
@interface AutofillSettingsMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<AutofillSettingsConsumer> consumer;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_
