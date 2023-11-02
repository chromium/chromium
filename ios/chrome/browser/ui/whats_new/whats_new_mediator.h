// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_mediator_consumer.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_primary_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_action_handler.h"

// Mediator between the Model and the UI.
// What's New mediator between `WhatsNewModel` and the view layers
// `WhatsNewTableViewController` and `WhatsNewDetailViewController`.
@interface WhatsNewMediator
    : NSObject <WhatsNewPrimaryActionHandler, WhatsNewTableViewActionHandler>

// The delegate object that manages interactions with What's New table view.
@property(nonatomic, weak) id<WhatsNewMediatorConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_MEDIATOR_H_
