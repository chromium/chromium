// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_FAVICON_FAVICON_VIEW_H_
#define IOS_CHROME_COMMON_UI_FAVICON_FAVICON_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {
// Minimum width and height of favicon.
const CGFloat kFaviconMinSize = 16.0f;
// Default width and height of favicon.
const CGFloat kFaviconPreferredSize = 24.0f;
}  // namespace

@interface FaviconView : UIView

// Configures this view with given attributes.
- (void)configureWithAttributes:(nullable FaviconAttributes*)attributes;
// Sets monogram font.
- (void)setFont:(nonnull UIFont*)font;

@end

#endif  // IOS_CHROME_COMMON_UI_FAVICON_FAVICON_VIEW_H_
