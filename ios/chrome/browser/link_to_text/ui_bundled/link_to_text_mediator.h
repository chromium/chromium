// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"

@protocol ActivityServiceCommands;
@protocol EditMenuAlertDelegate;

// Mediator that mediates between the browser container views and the
// link_to_text tab helpers.
@interface LinkToTextMediator : NSObject <EditMenuBuilder>

// The delegate to present error message alerts.
@property(nonatomic, weak) id<EditMenuAlertDelegate> alertDelegate;

// The handler for activity services commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;

// Returns whether the link to text feature should be offered for the current
// user selection in `webState`.
- (BOOL)shouldOfferLinkToTextInWebState:(web::WebState*)webState;

// Handles the link to text menu item selection in `webState`.
- (void)handleLinkToTextSelectionInWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_UI_BUNDLED_LINK_TO_TEXT_MEDIATOR_H_
