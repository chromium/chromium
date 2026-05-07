// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_delegate.h"

namespace actor {
class ActorService;
}

class WebStateList;

// The handler for Gemini actuations, bridging to the Actor orchestration layer.
@interface GeminiActuationHandler : NSObject <GeminiActuationDelegate>

// Initialize the handler with the ActorService and WebStateList.
- (instancetype)initWithActorService:(actor::ActorService*)actorService
                        webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_
