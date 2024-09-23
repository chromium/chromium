// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"

namespace feature_engagement {
class Tracker;
}

@protocol ContextualPanelEntrypointConsumer;
@protocol ContextualPanelEntrypointMediatorDelegate;
@protocol ContextualSheetCommands;
@protocol ContextualPanelEntrypointIPHCommands;
class WebStateList;

// Mediator for Contextual Panel Entrypoint.
@interface ContextualPanelEntrypointMediator
    : NSObject <ContextualPanelEntrypointMutator>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
         engagementTracker:(feature_engagement::Tracker*)engagementTracker
    contextualSheetHandler:(id<ContextualSheetCommands>)contextualSheetHandler
     entrypointHelpHandler:
         (id<ContextualPanelEntrypointIPHCommands>)entrypointHelpHandler
    NS_DESIGNATED_INITIALIZER;

// The consumer for this mediator.
@property(nonatomic, weak) id<ContextualPanelEntrypointConsumer> consumer;

// The delegate for this mediator.
@property(nonatomic, weak) id<ContextualPanelEntrypointMediatorDelegate>
    delegate;

// Cleanup and disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_
