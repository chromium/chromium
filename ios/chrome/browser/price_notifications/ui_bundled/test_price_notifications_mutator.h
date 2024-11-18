// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_TEST_PRICE_NOTIFICATIONS_MUTATOR_H_
#define IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_TEST_PRICE_NOTIFICATIONS_MUTATOR_H_

#import "ios/chrome/browser/price_notifications/ui_bundled/price_notifications_mutator.h"

@interface TestPriceNotificationsMutator : NSObject <PriceNotificationsMutator>

// Indicates whether the mediator would have been able to execute the given
// command.
@property(nonatomic, assign) BOOL didNavigateToItemPage;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_NOTIFICATIONS_UI_BUNDLED_TEST_PRICE_NOTIFICATIONS_MUTATOR_H_
