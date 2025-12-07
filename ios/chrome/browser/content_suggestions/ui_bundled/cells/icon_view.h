// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_H_

#import <UIKit/UIKit.h>

@class IconViewConfiguration;

// A view which contains an icon for a Safety Check item.
@interface IconView : UIView

// Instantiates an `IconView` given a `configuration`.
- (instancetype)initWithConfiguration:(IconViewConfiguration*)configuration;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_VIEW_H_
