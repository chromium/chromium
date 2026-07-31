// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace autofill {
class AtMemoryQueryService;
}

namespace web {
class WebState;
}

// Mediator for AtMemory search feature page.
@interface AtMemorySearchMediator : NSObject

// The designated initializer. `atMemoryQueryService` takes the string provided
// by the user and provides results to the user if available. If not, the
// service provides an empty result along with a status indicating the error.
// `webState` is used to retrieve context like the current URL and page title.
- (instancetype)initWithAtMemoryQueryService:
                    (autofill::AtMemoryQueryService*)atMemoryQueryService
                                    webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and stops any ongoing observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_SEARCH_MEDIATOR_H_
