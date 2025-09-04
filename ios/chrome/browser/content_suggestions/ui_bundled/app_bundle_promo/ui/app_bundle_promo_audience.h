// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_AUDIENCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_AUDIENCE_H_

// Interface to handle App Bundle promo card user events.
@protocol AppBundlePromoAudience

// Called when the promo is selected by the user.
- (void)didSelectAppBundlePromo;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_APP_BUNDLE_PROMO_UI_APP_BUNDLE_PROMO_AUDIENCE_H_
