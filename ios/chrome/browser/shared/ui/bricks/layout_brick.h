// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BRICKS_LAYOUT_BRICK_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BRICKS_LAYOUT_BRICK_H_

#import <UIKit/UIKit.h>

// A static helper class providing "layout" type view bricks.
@interface LayoutBrick : NSObject

// Pre-configured vertical UIStackView with 16pt standard item spacing.
+ (UIStackView*)contentStackWithSubviews:(NSArray<UIView*>*)subviews;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BRICKS_LAYOUT_BRICK_H_
