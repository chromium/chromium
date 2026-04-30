// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

// A custom view for the app bar that handles top-edge masking for rounded
// corners and touch transparency for the background area.
@interface AppBarBackgroundView : UIView

// Whether the app bar is in incognito mode.
@property(nonatomic, assign) BOOL incognito;

// Hides any color background if YES.
@property(nonatomic, assign) BOOL hideColorBackground;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_BACKGROUND_VIEW_H_
