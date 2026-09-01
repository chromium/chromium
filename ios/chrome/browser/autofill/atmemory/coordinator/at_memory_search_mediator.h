// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_mutator.h"

namespace autofill {
class AtMemoryManager;
class BrowserAutofillManager;
}

namespace personal_context {
class PersonalContextFirstRunService;
}

namespace web {
class WebState;
}

@protocol AtMemoryFillCommands;
@protocol AtMemorySearchConsumer;
@protocol AtMemoryCommands;
@protocol AtMemorySearchResultCommands;

// Mediator for AtMemory search feature page.
@interface AtMemorySearchMediator : NSObject <AtMemorySearchMutator>

// Handler for filling commands.
@property(nonatomic, weak) id<AtMemoryFillCommands> fillHandler;

// Handler for actions related to the AtMemory search results.
@property(nonatomic, weak) id<AtMemorySearchResultCommands> searchResultHandler;

// Handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

// The consumer for this mediator.
@property(nonatomic, weak) id<AtMemorySearchConsumer> consumer;

// The designated initializer. `atMemoryManager` provides access to the
// AtMemory manager. `autofillManager` provides the primary main frame autofill
// manager. `webState` is used to retrieve context like the UKM source ID.
// `firstRunService` is used to read and update notice confirmation states.
- (instancetype)
    initWithAtMemoryManager:(autofill::AtMemoryManager*)atMemoryManager
            autofillManager:(autofill::BrowserAutofillManager*)autofillManager
                   webState:(web::WebState*)webState
            firstRunService:(personal_context::PersonalContextFirstRunService*)
                                firstRunService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and stops any ongoing observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_
