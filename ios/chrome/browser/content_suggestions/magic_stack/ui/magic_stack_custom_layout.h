// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_CUSTOM_LAYOUT_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_CUSTOM_LAYOUT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_paging_layout_provider.h"

@class MagicStackModule;

// Custom layout for the Magic Stack that controls cell attributes during
// animations and provides horizontal paging and section sizing.
@interface MagicStackCustomLayout
    : UICollectionViewCompositionalLayout <MagicStackPagingLayoutProvider>

// Initializes the layout with default horizontal paging configuration.
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_CUSTOM_LAYOUT_H_
