// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_EVENTS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_EVENTS_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class NewTabPageCoordinator;
class WebStateList;
@protocol TabConsumer;
class SessionRestorationBrowserAgent;

// Mediator that handles tab events.
// The required dependencies are injected into the mediator instance on init,
// and are generally expected not to change during the mediator's lifetime.
// The mediator keeps only weak references to injected dependencies.
@interface TabEventsMediator : NSObject

// Consumer for tab UI changes.
@property(nonatomic, weak) id<TabConsumer> consumer;

// Creates an instance of the mediator. Observers will be installed into all
// existing web states in `webStateList`. While the mediator is alive,
// observers will be added and removed from web states when they are inserted
// into or removed from the web state list.
// TODO(crbug.com/1348459): Stop lazy loading in NTPCoordinator and remove this
// dependency.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      ntpCoordinator:(NewTabPageCoordinator*)ntpCoordinator
                    restorationAgent:(SessionRestorationBrowserAgent*)
                                         sessionRestorationBrowserAgent;

// Disconnects all observers set by the mediator on any web states in its
// web state list. After `disconnect` is called, the mediator will not add
// observers to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_EVENTS_MEDIATOR_H_
