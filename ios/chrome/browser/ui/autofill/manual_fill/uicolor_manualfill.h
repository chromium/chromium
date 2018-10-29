// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_

#import <UIKit/UIKit.h>

@interface UIColor (ManualFill)

// Color to set in interactable elements for manual fill (0.1, 0.45, 0.91 RGB).
@property(class, nonatomic, readonly) UIColor* cr_manualFillTintColor;

// Color for the line separators in manual fill (0.66, 0.66, 0.66 RGB).
@property(class, nonatomic, readonly) UIColor* cr_manualFillSeparatorColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_UICOLOR_MANUALFILL_H_
