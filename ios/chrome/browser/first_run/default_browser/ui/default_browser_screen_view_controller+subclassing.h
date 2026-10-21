// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_SUBCLASSING_H_

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller.h"

// Interface for concrete subclasses of DefaultBrowserScreenViewController.
@interface DefaultBrowserScreenViewController (Subclassing)

// Returns the banner image name for the promo screen.
// Subclasses can override this method to provide a custom banner image.
- (NSString*)bannerNameForPromo;

// Returns the default subtitle text when `subtitleText` is not explicitly set.
// Subclasses can override this method to provide a custom default subtitle.
- (NSString*)defaultSubtitleText;

// Returns the main content view to be added inside `self.specificContentView`.
// Subclasses can override this method to customize the promo content view.
- (UIView*)contentViewForPromo;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_SCREEN_VIEW_CONTROLLER_SUBCLASSING_H_
