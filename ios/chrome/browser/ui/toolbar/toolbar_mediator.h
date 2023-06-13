// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/prefs/pref_service.h"

class WebStateList;

/// Delegate for events in `ToolbarMediator`.
@protocol ToolbarMediatorDelegate <NSObject>

/// Updates toolbar appearance.
- (void)updateToolbar;

@end

@interface ToolbarMediator : NSObject

/// Delegate for events in `ToolbarMediator`.
@property(nonatomic, weak) id<ToolbarMediatorDelegate> delegate;
/// Observe user preference changes for preferred omnibox position.
@property(nonatomic, assign) PrefService* prefService;

/// Creates an instance of the mediator. Observers will be installed into all
/// existing web states in `webStateList`. While the mediator is alive,
/// observers will be added and removed from web states when they are inserted
/// into or removed from the web state list.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

/// Disconnects all observers set by the mediator on any web states in its
/// web state list. After `disconnect` is called, the mediator will not add
/// observers to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
