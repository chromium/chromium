// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_CONSUMER_H_

// Consumer protocol for the PrimaryToolbarMediator to update necessary UI.
@protocol PrimaryToolbarConsumer

// Shows the banner promo view.
- (void)showBannerPromo;

// Hides the banner promo view.
- (void)hideBannerPromo;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_PRIMARY_TOOLBAR_CONSUMER_H_
