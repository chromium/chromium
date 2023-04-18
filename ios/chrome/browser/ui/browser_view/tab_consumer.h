// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}  // namespace web

// Consumer allowing the Browser View Controller to be updated when there is a
// change in WebStateList.
@protocol TabConsumer <NSObject>

// Tells the consumer to reset the content view.
- (void)resetTab;

// Tells the consumer to start an animation for a background tab.
- (void)initiateNewTabBackgroundAnimation;

// Tells the consumer to start an animation for a foreground tab.
// TODO(crbug.com/1417570): Remove webState from this call.
- (void)initiateNewTabForegroundAnimationForWebState:(web::WebState*)webState;

// Tells the consumer to dismiss popups and modal dialogs that are displayed
// above the BVC upon size changes (e.g. rotation, resizing,…) or when the
// accessibility escape gesture is performed.
- (void)prepareForNewTabAnimation;

// Tells the consumer to make any required view changes when a `webState` is
// selected in the WebStateList. The notification will not be sent when the
// `webState` is already the selected WebState.
- (void)webStateSelected;

// Tells the consumer to make `webState` the currently visible WebState,
// displaying its view if BVC is in an active state.
// TODO(crbug.com/1417570): Remove webState from this call.
- (void)displayWebStateIfActive:(web::WebState*)webState;

// Tells the consumer to display the tab view associated to the new web state
// index.
- (void)switchtoTabWithNewWebStateIndex:(NSInteger)newWebStateIndex;

// Tells the consumer to remove any bookmark modal controller from view if
// visible.
- (void)dismissBookmarkModalController;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_CONSUMER_H_
