// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BLUE_DOT_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BLUE_DOT_UTIL_H_

#import <UIKit/UIKit.h>

// Updates the layer mask of `viewToMask` to punch a hole for a blue dot in its
// top trailing corner. `hasBlueDot` controls whether the mask is applied or
// removed.
void UpdateBlueDotMaskForView(UIView* viewToMask, BOOL hasBlueDot);

// Creates, configures, and adds a blue dot view to the top trailing corner of
// `button`. The dot is inserted below the button's image view in the hierarchy
// if present. Returns the created blue dot view.
UIView* ConfigureAndAddBlueDotView(UIButton* button);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_BLUE_DOT_UTIL_H_
