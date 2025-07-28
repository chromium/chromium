// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_INTERACTIVE_PROMO_RESULTS_PAGE_PRESENTER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_INTERACTIVE_PROMO_RESULTS_PAGE_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenting.h"

@class LensOverlayContainerViewController;
@class LensResultPageViewController;

// A simplified presenter for the Lens results bottom sheet, designed
// specifically for the interactive First Run Experience (FRE) promo.
//
// Unlike the more general-purpose `LensOverlayResultsPagePresenter`, this class
// operates on a set of simplifying assumptions tailored for its specific use
// case:
// - The presentation is designed for iPhone in portrait orientation only. It
// does not support iPad layouts or landscape mode.
// - It assumes the initial search is performed on an image without selectable
// text, meaning it does not need to handle translate-specific UI or results.
// - The results are shown in a simple bottom sheet with a fixed height (50% of
// the container) and does not support multiple detents or complex sheet
// interactions.
@interface LensInteractivePromoResultsPagePresenter
    : NSObject <LensOverlayResultsPagePresenting>

// Initializes the presenter with `baseViewController` and
// `resultViewController`.
- (instancetype)initWithBaseViewController:
                    (LensOverlayContainerViewController*)baseViewController
                  resultPageViewController:
                      (LensResultPageViewController*)resultViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_INTERACTIVE_PROMO_RESULTS_PAGE_PRESENTER_H_
