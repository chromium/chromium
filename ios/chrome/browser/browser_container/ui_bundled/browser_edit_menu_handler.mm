// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_handler.h"

@implementation BrowserEditMenuHandler

- (void)buildEditMenuWithBuilder:(id<UIMenuBuilder>)builder
                      inWebState:(web::WebState*)webState {
  [self.linkToTextDelegate buildEditMenuWithBuilder:builder
                                         inWebState:webState];
  [self.searchWithDelegate buildEditMenuWithBuilder:builder
                                         inWebState:webState];
  [self.explainWithGeminiDelegate buildEditMenuWithBuilder:builder
                                                inWebState:webState];
  [self.partialTranslateDelegate buildEditMenuWithBuilder:builder
                                               inWebState:webState];
  [self.dataControlsDelegate buildEditMenuWithBuilder:builder
                                           inWebState:webState];
}

@end
