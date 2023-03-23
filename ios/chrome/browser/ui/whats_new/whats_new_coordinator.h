// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_delegate.h"

@protocol PromosManagerUIHandler;

@interface WhatsNewCoordinator : ChromeCoordinator <WhatsNewTableViewDelegate>

// Whether to show a promo bubble after dismissing What's New.
@property(nonatomic, assign) BOOL shouldShowBubblePromoOnDismiss;

// The promos manager ui handler to alert for promo UI changes. Should only be
// set if this coordinator was a promo presented by the PromosManager.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_
