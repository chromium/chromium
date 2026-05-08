// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_IPH_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_IPH_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// A view that draws a circular gradient background for the App Bar when the IPH
// is displayed.
@interface AppBarIPHBackgroundView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Whether the gradient is centered (YES) or left-bottom aligned (NO).
@property(nonatomic, assign) BOOL centered;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_IPH_BACKGROUND_VIEW_H_
