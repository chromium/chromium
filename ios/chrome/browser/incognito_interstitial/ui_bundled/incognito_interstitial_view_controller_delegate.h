// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_DELEGATE_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@protocol IncognitoInterstitialViewControllerDelegate <
    PromoStyleViewControllerDelegate>

// Called back when the user interacts with the "Cancel" button in the
// controlled view.
- (void)didTapCancelButton;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_DELEGATE_H_
