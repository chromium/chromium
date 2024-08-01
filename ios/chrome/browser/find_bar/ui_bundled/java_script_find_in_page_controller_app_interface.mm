// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/java_script_find_in_page_controller_app_interface.h"

#import "ios/chrome/browser/find_in_page/model/java_script_find_in_page_controller.h"

@implementation JavaScriptFindInPageControllerAppInterface

+ (void)clearSearchTerm {
  [JavaScriptFindInPageController setSearchTerm:nil];
}

@end
