// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WHATS_NEW_COORDINATOR_WHATS_NEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_WHATS_NEW_COORDINATOR_WHATS_NEW_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PromosManagerUIHandler;

@interface WhatsNewCoordinator : ChromeCoordinator

// Should only be called if this coordinator was a promo presented by the
// PromosManager. The promos manager ui handler to alert for promo UI changes.
- (void)setShouldShowPromoOnDismissWithHandler:
    (id<PromosManagerUIHandler>)promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_WHATS_NEW_COORDINATOR_WHATS_NEW_COORDINATOR_H_
