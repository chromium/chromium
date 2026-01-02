// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_GEMINI_COORDINATOR_ASSISTANT_GEMINI_MEDIATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_GEMINI_COORDINATOR_ASSISTANT_GEMINI_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/coordinator/assistant_commands.h"

@protocol AssistantGeminiConsumer;

// Mediator for the Gemini feature.
@interface AssistantGeminiMediator : NSObject

// The consumer view controller.
@property(nonatomic, weak) id<AssistantGeminiConsumer> consumer;

// Handler for assistant commands.
@property(nonatomic, weak) id<AssistantCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_GEMINI_COORDINATOR_ASSISTANT_GEMINI_MEDIATOR_H_
