// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_table_view_delegate.h"

@interface WhatsNewCoordinator : ChromeCoordinator <WhatsNewTableViewDelegate>

// Whether to show a promo bubble after dismissing What's New.
@property(nonatomic, assign) BOOL shouldShowBubblePromoOnDismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_COORDINATOR_H_