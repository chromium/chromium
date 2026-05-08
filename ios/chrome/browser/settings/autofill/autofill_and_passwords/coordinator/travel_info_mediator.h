// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/travel_info_table_view_controller.h"

namespace autofill {
class EntityDataManager;
}

@protocol TravelInfoConsumer;
@class TravelInfoMediator;

@protocol TravelInfoMediatorDelegate <NSObject>

// Requests the coordinator to open the editor for the specified entity ID.
- (void)travelInfoMediator:(TravelInfoMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID;

@end

// Mediator for the Travel Info settings page.
@interface TravelInfoMediator : NSObject <TravelInfoMutator>

// Delegate for navigation requests.
@property(nonatomic, weak) id<TravelInfoMediatorDelegate> delegate;

// Consumer for this mediator.
@property(nonatomic, weak) id<TravelInfoConsumer> consumer;

- (instancetype)initWithEntityDataManager:
    (autofill::EntityDataManager*)entityDataManager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_TRAVEL_INFO_MEDIATOR_H_
