// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_THUMB_STRIP_PLUS_SIGN_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_THUMB_STRIP_PLUS_SIGN_BUTTON_H_

#import <UIKit/UIKit.h>

// The button that sticks to the right of the screen when the thumb strip is
// visible. It has a plus sign and a transparency gradient.
@interface ThumbStripPlusSignButton : UIButton
// The image view with a plus sign.
@property(nonatomic, strong) UIImageView* plusSignImage;
// Extra vertical offset for the + sign image.
@property(nonatomic, assign) CGFloat plusSignVerticalOffset;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_THUMB_STRIP_PLUS_SIGN_BUTTON_H_
