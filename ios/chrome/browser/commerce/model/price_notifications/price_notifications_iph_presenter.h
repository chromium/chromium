// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_PRESENTER_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_PRESENTER_H_

// Protocol to present in-product help (IPH) related to the price notifications
// feature.
@protocol PriceNotificationsIPHPresenter

// Tells receiver to present the in-product help (IPH) to price track the
// currently browsed site.
- (void)presentPriceNotificationsWhileBrowsingIPH;

@end

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_IPH_PRESENTER_H_
