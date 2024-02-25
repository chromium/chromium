// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"

@protocol ContextualPanelEntrypointConsumer;
@protocol ContextualPanelEntrypointMediatorDelegate;

// Mediator for Contextual Panel Entrypoint.
@interface ContextualPanelEntrypointMediator
    : NSObject <ContextualPanelEntrypointMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<ContextualPanelEntrypointConsumer> consumer;

// The delegate for this mediator.
@property(nonatomic, weak) id<ContextualPanelEntrypointMediatorDelegate>
    delegate;

- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_ENTRYPOINT_COORDINATOR_CONTEXTUAL_PANEL_ENTRYPOINT_MEDIATOR_H_
