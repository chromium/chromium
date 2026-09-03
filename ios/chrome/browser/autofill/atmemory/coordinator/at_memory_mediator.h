// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/common/unique_ids.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"

@protocol AtMemoryCommands;
@protocol ManualFillContentInjector;

namespace autofill {
class AtMemoryManager;
class BrowserAutofillManager;
}  // namespace autofill

// Mediator for AtMemory, serving as a mutator for fill operations.
@interface AtMemoryMediator : NSObject <AtMemoryFillCommands>

// Handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

// The designated initializer. `atMemoryManager` provides access to the
// AtMemory manager. `autofillManager` provides the primary main frame autofill
// manager. `contentInjector` is used to inject content into the web page.
// `fieldId` is the identifier of the field that initiated the AtMemory flow.
- (instancetype)
    initWithAtMemoryManager:(autofill::AtMemoryManager*)atMemoryManager
            autofillManager:(autofill::BrowserAutofillManager*)autofillManager
            contentInjector:(id<ManualFillContentInjector>)contentInjector
                    fieldId:(autofill::FieldGlobalId)fieldId
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and clears its references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
