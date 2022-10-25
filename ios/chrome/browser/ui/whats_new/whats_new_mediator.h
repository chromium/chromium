// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_

#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_action_handler.h"

@protocol WhatsNewMediatorConsumer;
@protocol ApplicationCommands;

class UrlLoadingBrowserAgent;

// Mediator between the Model and the UI.
// What's New mediator between `WhatsNewModel` and the view layers
// `WhatsNewTableViewController` and `WhatsNewDetailViewController`.
@interface WhatsNewMediator
    : NSObject <WhatsNewDetailViewActionHandler, WhatsNewTableViewActionHandler>

// The delegate object that manages interactions with What's New table view.
@property(nonatomic, weak) id<WhatsNewMediatorConsumer> consumer;

// Url loading agent.
@property(nonatomic, assign) UrlLoadingBrowserAgent* urlLoadingAgent;

// Browser coordinator command handler.
@property(nonatomic, weak) id<ApplicationCommands> handler;

// The view controller that presents the popup menu.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_
