// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_MEDIATOR_H_

#import <Foundation/Foundation.h>
class PrefService;

namespace autofill {
class EntityDataManager;
}

@protocol AutofillAndPasswordsConsumer;

// Mediator for the Autofill and Passwords settings page.
@interface AutofillAndPasswordsMediator : NSObject

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                      entityDataManager:
                          (autofill::EntityDataManager*)entityDataManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<AutofillAndPasswordsConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_MEDIATOR_H_
