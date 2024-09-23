// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/custom_highlight_button.h"

// Button with touch target potentially extended outside its bound in a circle
// with the center in the center of this button.
@interface ExtendedTouchTargetButton : CustomHighlightableButton

// The minimum diameter to extend to. Default is 44 point, per Apple UI
// Guidelines.
@property(nonatomic, assign) CGFloat minimumDiameter;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_
