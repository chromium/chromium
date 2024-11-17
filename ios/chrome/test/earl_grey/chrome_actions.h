// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_

#import <Foundation/Foundation.h>

#import <UIKit/UIKit.h>

#include <string>

@class ElementSelector;
@protocol GREYAction;

namespace chrome_test_util {

// Action to longpress on the element selected by `selector` in the Chrome's
// webview. If `triggers_context_menu` is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If `triggers_context_menu` is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
id<GREYAction> LongPressElementForContextMenu(ElementSelector* selector,
                                              bool triggers_context_menu);

// Action to scroll a web element described by the given `selector` to visible
// on the current web state.
id<GREYAction> ScrollElementToVisible(ElementSelector* selector);

// Action to turn the switch of a TableViewSwitchCell to the given `on` state.
id<GREYAction> TurnTableViewSwitchOn(BOOL on);

// Action to tap a web element described by the given `selector` on the current
// web state.
// Checks the effect of the tap using JavaScript.
id<GREYAction> TapWebElement(ElementSelector* selector);

// Action to tap a web element described by the given `selector` on the current
// web state.
// Does not check the effect of the tap. This function is expected to be use
// when the effect of the tap is on the browser side (e.g. showing a popup).
id<GREYAction> TapWebElementUnverified(ElementSelector* selector);

// Action to tap a web element with id equal to `element_id` on the current web
// state.
id<GREYAction> TapWebElementWithId(const std::string& element_id);

// Action to tap a web element in iframe with the given `element_id` on the
// current web state. iframe is an immediate child of the main frame with the
// given index. The action fails if target iframe has a different origin from
// the main frame.
id<GREYAction> TapWebElementWithIdInFrame(const std::string& element_id,
                                          const int frame_index);

// Action to long press on the center of an element. This is mostly to be used
// when the element is occulted by something and so the grey_longPress action
// would fail.
id<GREYAction> LongPressOnHiddenElement();

// Action to scroll to top of a UIScrollView.
// On iOS 13 the settings menu appears as a card that can be dismissed with a
// downward swipe, for this reason we need to swipe up programmatically to
// avoid dismissing the VC.
id<GREYAction> ScrollToTop();

// Action to tap an element at the given xOriginStartPercentage as a percentage
// of the total width and yOriginStartPercentage as a percentage of the total
// height. Percentages are between 0 and 1, where 1 is 100%.
id<GREYAction> TapAtPointPercentage(CGFloat xOriginStartPercentage,
                                    CGFloat yOriginStartPercentage);

// Action to swipe a TableViewCell enough to display the "Delete" button and
// not too much to have the cell being deleted right away.
id<GREYAction> SwipeToShowDeleteButton();

// Action to simulate the behaviour of swiping right using the 3-finger gesture
// with VoiceOver. This gesture "jump" to the next screen of the scroll view. To
// simulate it, it is changing the content offset and triggering scroll view
// delegate methods as there is no way to actually trigger the gesture.
id<GREYAction> AccessibilitySwipeRight();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_
