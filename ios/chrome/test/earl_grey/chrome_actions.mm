// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_actions.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ChromeActionsAppInterface)
#endif

namespace chrome_test_util {

id<GREYAction> LongPressElementForContextMenu(ElementSelector* selector,
                                              bool triggers_context_menu) {
  return [ChromeActionsAppInterface longPressElement:selector
                                  triggerContextMenu:triggers_context_menu];
}

id<GREYAction> ScrollElementToVisible(ElementSelector* selector) {
  return [ChromeActionsAppInterface scrollElementToVisible:selector];
}

id<GREYAction> TurnSettingsSwitchOn(BOOL on) {
  return [ChromeActionsAppInterface turnSettingsSwitchOn:on];
}

id<GREYAction> TurnSyncSwitchOn(BOOL on) {
  return [ChromeActionsAppInterface turnSyncSwitchOn:on];
}

id<GREYAction> TapWebElement(ElementSelector* selector) {
  return [ChromeActionsAppInterface tapWebElement:selector];
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

}  // namespace chrome_test_util
