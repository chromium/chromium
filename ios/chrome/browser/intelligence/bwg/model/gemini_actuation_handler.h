// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_chat_message_handler.h"

namespace actor {
class ActorService;
}

class WebStateList;

// The handler for Gemini actuations, bridging to the Actor orchestration layer.
// Also handles user chat messages, which may answer a pending task request.
@interface GeminiActuationHandler
    : NSObject <GeminiActuationDelegate, GeminiChatMessageHandler>

// Initialize the handler with the ActorService, WebStateList and Browser ID.
- (instancetype)initWithActorService:(actor::ActorService*)actorService
                        webStateList:(WebStateList*)webStateList
                           browserId:(SessionID)browserId
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects active request callbacks, failing them with `kExecutorDestroyed`.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_HANDLER_H_
