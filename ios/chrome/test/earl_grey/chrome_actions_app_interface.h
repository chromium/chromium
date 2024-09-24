// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import <UIKit/UIKit.h>

@class ElementSelector;
@protocol GREYAction;

// Helper class to return actions for EG tests.  These helpers are compiled
// into the app binary and can be called from either app or test code.
@interface ChromeActionsAppInterface : NSObject

// Action to longpress on the element selected by `selector` in the Chrome's
// webview. If `triggers_context_menu` is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If `triggers_context_menu` is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
+ (id<GREYAction>)longPressElement:(ElementSelector*)selector
                triggerContextMenu:(BOOL)triggerContextMenu;

// Action to scroll a web element described by the given `selector` to visible
// on the current web state.
+ (id<GREYAction>)scrollElementToVisible:(ElementSelector*)selector;

// Action to turn the switch of a TableViewSwitchCell to the given `on` state.
+ (id<GREYAction>)turnTableViewSwitchOn:(BOOL)on;

// Action to tap a web element described by the given `selector` on the current
// web state.
// Checks the effect of the tap using JavaScript.
+ (id<GREYAction>)tapWebElement:(ElementSelector*)selector;

// Action to tap a web element described by the given `selector` on the current
// web state.
// Does not check the effect of the tap. This function is expected to be use
// when the effect of the tap is on the browser side (e.g. showing a popup).
+ (id<GREYAction>)tapWebElementUnverified:(ElementSelector*)selector;

// Action to long press on the center of an element. This is mostly to be used
// when the element is occulted by something and so the grey_longPress action
// would fail.
+ (id<GREYAction>)longPressOnHiddenElement;

// Action to scroll to top of a collection.
// On iOS 13 the settings menu appears as a card that can be dismissed with a
// downward swipe, for this reason we need to swipe up programatically to
// avoid dismissing the VC.
+ (id<GREYAction>)scrollToTop;

// Action to tap an element at the given xOriginStartPercentage as a percentage
// of the total width and yOriginStartPercentage as a percentage of the total
// height. Percentages are between 0 and 1, where 1 is 100%.
+ (id<GREYAction>)tapAtPointAtxOriginStartPercentage:(CGFloat)x
                              yOriginStartPercentage:(CGFloat)y;

// Action to swipe a TableViewCell enough to display the "Delete" button and
// not too much to have the cell being deleted right away.
+ (id<GREYAction>)swipeToShowDeleteButton;

// Action to simulate the behaviour of swiping right using the 3-finger gesture
// with VoiceOver. To simulate it, it is changing the content offset and
// triggering scroll view delegate methods as there is no way to actually
// trigger the gesture.
+ (id<GREYAction>)accessibilitySwipeRight;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_APP_INTERFACE_H_
