// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_AIM_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
#define IOS_CHROME_BROWSER_ASSISTANT_AIM_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/coordinator/assistant_commands.h"

@protocol AssistantAIMConsumer;

// Mediator for the AIM feature.
@interface AssistantAIMMediator : NSObject

// The consumer view controller.
@property(nonatomic, weak) id<AssistantAIMConsumer> consumer;

// Handler for assistant commands.
@property(nonatomic, weak) id<AssistantCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_AIM_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
