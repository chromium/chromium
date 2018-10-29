// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"

// Protected interface of the AdaptiveToolbarViewController.
@interface AdaptiveToolbarViewController (Subclassing)

// Sets the progress of the progressBar to 1 then hides it.
- (void)stopProgressBar;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_CONTROLLER_SUBCLASSING_H_
