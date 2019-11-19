// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_actions_app_interface.h"

#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShellActionsAppInterface

+ (id<GREYAction>)longPressElementForContextMenu:(ElementSelector*)selector {
  return WebViewLongPressElementForContextMenu(
      web::shell_test_util::GetCurrentWebState(), selector, true);
}

@end
