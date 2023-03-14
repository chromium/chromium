// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_LOADING_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_LOADING_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"

// Displays an activity indicator with an optional message over a
// clearBackground.
@interface TableViewLoadingView : UIView

- (instancetype)initWithFrame:(CGRect)frame
               loadingMessage:(NSString*)message NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Call this method when this view is added to the visible view hierarchy.
// An activity indicator will be presented if this view is still in the view
// hierarchy at that time.
- (void)startLoadingIndicator;

// Call this method when this view is removed from the visible view hierarchy.
// `completion` will be called when this view is done animating out, and can be
// nil.
- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_LOADING_VIEW_H_
