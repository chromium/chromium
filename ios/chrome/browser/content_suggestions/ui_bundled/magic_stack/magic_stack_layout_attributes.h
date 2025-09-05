// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_LAYOUT_ATTRIBUTES_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_LAYOUT_ATTRIBUTES_H_

#import <UIKit/UIKit.h>

// Custom layout attributes for the Magic Stack. Used because some cells contain
// UIVisualEffectView, and the default animation for inserting/removing cells
// changes the cell's alpha, which does not work with UIVisualEffectView.
@interface MagicStackLayoutAttributes : UICollectionViewLayoutAttributes

// The alpha value for the cell's subviews.
@property(nonatomic, assign) CGFloat subviewAlpha;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_MAGIC_STACK_MAGIC_STACK_LAYOUT_ATTRIBUTES_H_
