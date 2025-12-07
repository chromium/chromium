// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_H_

#import <UIKit/UIKit.h>

class ToolbarsSizeObserver;

// TODO(crbug.com/438115053): Rename ToolbarsSize to ToolbarSizes.
// Toolbars' size. Almost constants.
@interface ToolbarsSize : NSObject

// Define properties as readwrite.
@property(nonatomic, assign) CGFloat collapsedTopToolbarHeight;
@property(nonatomic, assign) CGFloat expandedTopToolbarHeight;
@property(nonatomic, assign) CGFloat expandedBottomToolbarHeight;
@property(nonatomic, assign) CGFloat collapsedBottomToolbarHeight;

- (instancetype)
    initWithCollapsedTopToolbarHeight:(CGFloat)collapsedTopToolbarHeight
             expandedTopToolbarHeight:(CGFloat)expandedTopToolbarHeight
          expandedBottomToolbarHeight:(CGFloat)expandedBottomToolbarHeight
         collapsedBottomToolbarHeight:(CGFloat)collapsedBottomToolbarHeight;

// Setter for toolbars properties and notify observers.
- (void)setCollapsedTopToolbarHeight:(CGFloat)collapsedTopToolbarHeight
            expandedTopToolbarHeight:(CGFloat)expandedTopToolbarHeight
         expandedBottomToolbarHeight:(CGFloat)expandedBottomToolbarHeight
        collapsedBottomToolbarHeight:(CGFloat)collapsedBottomToolbarHeight;

// Adding and Removing observers.
- (void)addObserver:(ToolbarsSizeObserver*)observer;
- (void)removeObserver:(ToolbarsSizeObserver*)observer;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_H_
