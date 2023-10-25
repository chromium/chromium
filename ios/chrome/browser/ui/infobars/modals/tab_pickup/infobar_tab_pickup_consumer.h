// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_CONSUMER_H_

// The consumer protocol for the tab pickup modal.
@protocol InfobarTabPickupConsumer

// Called when the value of prefs::kTabPickupEnabled changed.
- (void)setTabPickupEnabled:(BOOL)enabled;

// Called when the tab-sync state changed.
- (void)setTabSyncEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_CONSUMER_H_
