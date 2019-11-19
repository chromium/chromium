// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_

#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"

// A button for an Infobar that contains a badge image.
@interface InfobarBadgeButton : ExtendedTouchTargetButton

// Sets the badge color to blue if |active| is YES, light gray if |active| is
// NO. Will animate change if |animated| is YES.
- (void)setActive:(BOOL)active animated:(BOOL)animated;
// Displays the badge button if |display| is YES, shows it if |display| is NO.
// Will animate change if |animated| is YES.
- (void)displayBadge:(BOOL)display animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_
