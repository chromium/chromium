// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_ITEM_IDENTIFIER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_ITEM_IDENTIFIER_H_

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Enum representing the different types of notifications in the notifications
// settings page.
enum NotificationsItemIdentifier {
  ItemIdentifierContent = kItemTypeEnumZero,
  ItemIdentifierTips,
  ItemIdentifierTipsNotificationsFooter,
  ItemIdentifierPriceTracking,
  ItemIdentifierSafetyCheck,
  ItemIdentifierSendTab,
  ItemIdentifierMaxValue = ItemIdentifierSendTab,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_ITEM_IDENTIFIER_H_
