// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_

#import <UIKit/UIKit.h>

// Button with touch target extended to at least 44 point diameter circle with
// the center in the center of this button, per Apple UI Guidelines.
@interface ExtendedTouchTargetButton : UIButton
@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_EXTENDED_TOUCH_TARGET_BUTTON_H_
