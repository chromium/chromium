// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_actions.h"

#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_actions_app_interface.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ShellActionsAppInterface)
#endif

namespace web {

id<GREYAction> LongPressElementForContextMenu(ElementSelector* selector) {
  return [ShellActionsAppInterface longPressElementForContextMenu:selector];
}

}  // namespace web
