// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_DIAMOND_GRID_BUTTON_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_DIAMOND_GRID_BUTTON_H_

#import <UIKit/UIKit.h>

class WebStateList;

// Button to enter/leave the tab grid in the Diamond prototype.
@interface DiamondGridButton : UIButton

// Sets the button up.
- (void)setup;

// Configures the button to display the count of `webStateList`.
- (void)configureWithWebStateList:(WebStateList*)webStateList;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_DIAMOND_GRID_BUTTON_H_
