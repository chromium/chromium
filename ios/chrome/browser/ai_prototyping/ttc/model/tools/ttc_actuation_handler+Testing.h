// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_TESTING_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_TESTING_H_

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

namespace actor {
class ActorService;
}  // namespace actor

// Testing category exposing internal hooks for unit tests.
@interface TTCActuationHandler (Testing)

// Overrides the ActorService for unit testing.
// @param actorService The mock or fake `ActorService` instance.
- (void)setActorServiceForTesting:(actor::ActorService*)actorService;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_TESTING_H_
