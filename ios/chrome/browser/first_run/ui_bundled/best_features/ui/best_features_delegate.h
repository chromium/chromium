// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_DELEGATE_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_DELEGATE_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@class BestFeaturesItem;

// Protocol to communicate user actions on the Best Features screen.
@protocol BestFeaturesDelegate <PromoStyleViewControllerDelegate>

// Called when user taps on a Best Feature item.
- (void)didTapBestFeaturesItem:(BestFeaturesItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_DELEGATE_H_
