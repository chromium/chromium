// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"

namespace chrome_test_util {

id<GREYAction> LongPressElementForContextMenu(ElementSelector* selector,
                                              bool triggers_context_menu) {
  return [ChromeActionsAppInterface longPressElement:selector
                                  triggerContextMenu:triggers_context_menu];
}

id<GREYAction> ScrollElementToVisible(ElementSelector* selector) {
  return [ChromeActionsAppInterface scrollElementToVisible:selector];
}

id<GREYAction> TurnTableViewSwitchOn(BOOL on) {
  return [ChromeActionsAppInterface turnTableViewSwitchOn:on];
}

id<GREYAction> TapWebElement(ElementSelector* selector) {
  return [ChromeActionsAppInterface tapWebElement:selector];
}

id<GREYAction> TapWebElementUnverified(ElementSelector* selector) {
  return [ChromeActionsAppInterface tapWebElementUnverified:selector];
}

id<GREYAction> TapWebElementWithId(const std::string& element_id) {
  return [ChromeActionsAppInterface
      tapWebElement:[ElementSelector selectorWithElementID:element_id]];
}

id<GREYAction> TapWebElementWithIdInFrame(const std::string& element_id,
                                          const int frame_index) {
  return [ChromeActionsAppInterface
      tapWebElement:[ElementSelector selectorWithElementID:element_id
                                          inFrameWithIndex:frame_index]];
}

id<GREYAction> LongPressOnHiddenElement() {
  return [ChromeActionsAppInterface longPressOnHiddenElement];
}

id<GREYAction> ScrollToTop() {
  return [ChromeActionsAppInterface scrollToTop];
}

id<GREYAction> TapAtPointPercentage(CGFloat xOriginStartPercentage,
                                    CGFloat yOriginStartPercentage) {
  return [ChromeActionsAppInterface
      tapAtPointAtxOriginStartPercentage:xOriginStartPercentage
                  yOriginStartPercentage:yOriginStartPercentage];
}

id<GREYAction> SwipeToShowDeleteButton() {
  return [ChromeActionsAppInterface swipeToShowDeleteButton];
}

id<GREYAction> AccessibilitySwipeRight() {
  return [ChromeActionsAppInterface accessibilitySwipeRight];
}

}  // namespace chrome_test_util
