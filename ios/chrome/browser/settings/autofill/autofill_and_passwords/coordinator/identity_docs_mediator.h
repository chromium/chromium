// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/identity_docs_table_view_controller.h"

namespace autofill {
class EntityDataManager;
}

@protocol IdentityDocsConsumer;
@class IdentityDocsMediator;

@protocol IdentityDocsMediatorDelegate <NSObject>

// Requests the coordinator to open the editor for the specified entity ID.
- (void)identityDocsMediator:(IdentityDocsMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID;

@end

// Mediator for the Identity Docs settings page.
@interface IdentityDocsMediator : NSObject <IdentityDocsMutator>

// Delegate for navigation requests.
@property(nonatomic, weak) id<IdentityDocsMediatorDelegate> delegate;

// Consumer for this mediator.
@property(nonatomic, weak) id<IdentityDocsConsumer> consumer;

- (instancetype)initWithEntityDataManager:
    (autofill::EntityDataManager*)entityDataManager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_IDENTITY_DOCS_MEDIATOR_H_
