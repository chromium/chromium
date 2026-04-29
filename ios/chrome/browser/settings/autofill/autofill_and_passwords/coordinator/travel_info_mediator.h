// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol TravelInfoConsumer;

// Mediator for the Travel Info settings page.
@interface TravelInfoMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<TravelInfoConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_
