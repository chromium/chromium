// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_UI_AGE_MISMATCH_SIGNOUT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SIGNIN_UI_AGE_MISMATCH_SIGNOUT_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// View controller for the Age Mismatch prompt.
// This prompt is displayed after a signout, when Chrome detects that the device
// is set up for a child but the Google account signing in to Chrome is not.
@interface AgeMismatchSignoutViewController : PromoStyleViewController

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_UI_AGE_MISMATCH_SIGNOUT_VIEW_CONTROLLER_H_
