// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class ElementSelector;
@protocol GREYAction;

// Helper class to return actions for EG tests.  These helpers are compiled
// into the app binary and can be called from either app or test code.
@interface ChromeActionsAppInterface : NSObject

// Action to longpress on the element selected by |selector| in the Chrome's
// webview. If |triggers_context_menu| is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If |triggers_context_menu| is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
+ (id<GREYAction>)longPressElement:(ElementSelector*)selector
                triggerContextMenu:(BOOL)triggerContextMenu;

// Action to scroll a web element described by the given |selector| to visible
// on the current web state.
+ (id<GREYAction>)scrollElementToVisible:(ElementSelector*)selector;

// Action to turn the switch of a SettingsSwitchCell to the given |on| state.
+ (id<GREYAction>)turnSettingsSwitchOn:(BOOL)on;

// Action to turn the switch of a SyncSwitchCell to the given |on| state.
+ (id<GREYAction>)turnSyncSwitchOn:(BOOL)on;

// Action to tap a web element described by the given |selector| on the current
// web state.
+ (id<GREYAction>)tapWebElement:(ElementSelector*)selector;

// Action to scroll to top of a collection.
// On iOS 13 the settings menu appears as a card that can be dismissed with a
// downward swipe, for this reason we need to swipe up programatically to
// avoid dismissing the VC.
+ (id<GREYAction>)scrollToTop;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_
