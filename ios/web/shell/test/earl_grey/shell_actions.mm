// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_actions.h"

#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/shell/test/earl_grey/shell_actions_app_interface.h"

namespace web {

id<GREYAction> LongPressElementForContextMenu(ElementSelector* selector) {
  return [ShellActionsAppInterface longPressElementForContextMenu:selector];
}

}  // namespace web
