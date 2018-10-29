// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

#include "ios/web/public/test/element_selector.h"

namespace chrome_test_util {

// Action to longpress on the element selected by |selector| in the Chrome's
// webview. If |triggers_context_menu| is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If |triggers_context_menu| is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
id<GREYAction> LongPressElementForContextMenu(
    web::test::ElementSelector selector,
    bool triggers_context_menu);

// Action to turn the switch of a LegacySettingsSwitchCell to the given |on|
// state.
id<GREYAction> TurnSettingsSwitchOn(BOOL on);

// Action to turn the switch of a SyncSwitchCell to the given |on| state.
id<GREYAction> TurnSyncSwitchOn(BOOL on);

// Action to tap a web element with id equal to |element_id| on the current web
// state.
id<GREYAction> TapWebElement(const std::string& element_id);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_ACTIONS_H_
