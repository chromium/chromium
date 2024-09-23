// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"

#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_delegate.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"
#import "ios/chrome/browser/ui/search_with/search_with_delegate.h"

@implementation BrowserEditMenuHandler

- (void)buildMenuWithBuilder:(id<UIMenuBuilder>)builder {
  [self.linkToTextDelegate buildMenuWithBuilder:builder];
  [self.searchWithDelegate buildMenuWithBuilder:builder];
  [self.partialTranslateDelegate buildMenuWithBuilder:builder];
}

@end
