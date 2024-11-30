// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_handler.h"

#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_delegate.h"
#import "ios/chrome/browser/search_with/ui_bundled/search_with_delegate.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"

@implementation BrowserEditMenuHandler

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [self.linkToTextDelegate buildMenuWithBuilder:builder];
  [self.searchWithDelegate buildMenuWithBuilder:builder];
  [self.partialTranslateDelegate buildMenuWithBuilder:builder];
}

@end
