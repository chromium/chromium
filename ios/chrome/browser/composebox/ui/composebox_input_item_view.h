// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"

// The aim input item view.
@interface ComposeboxInputItemView : UIView

/// Updates the UI based on the item's type.
- (void)configureWithItem:(ComposeboxInputItem*)item
                    theme:(ComposeboxTheme*)theme;

/// Prepares the view for reuse.
- (void)prepareForReuse;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_ITEM_VIEW_H_
