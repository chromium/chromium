// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_block_modulator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation PanelBlockModulator {
  base::WeakPtr<Browser> _browser;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         itemConfiguration:
                             (base::WeakPtr<ContextualPanelItemConfiguration>)
                                 itemConfiguration {
  if ((self = [super init])) {
    _baseViewController = viewController;
    if (browser) {
      _browser = browser->AsWeakPtr();
    }
    _itemConfiguration = itemConfiguration;
  }
  return self;
}

- (Browser*)browser {
  return _browser.get();
}

- (PanelBlockData*)panelBlockData {
  return nil;
}

- (NSString*)blockType {
  if (!_itemConfiguration) {
    return nil;
  }
  return base::SysUTF8ToNSString(
      StringForItemType(_itemConfiguration->item_type));
}

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end
