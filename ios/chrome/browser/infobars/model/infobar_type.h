// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_TYPE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_TYPE_H_

// Message Infobars types. Since these are used for metrics, entries should not
// be renumbered and numeric values should never be reused.
enum class InfobarType {
  // Generic Infobar that contains a Message and an action Button.
  kInfobarTypeConfirm = 0,
  // Message Infobar for Saving a password.
  kInfobarTypePasswordSave = 1,
  // Message Infobar for Updating a password.
  kInfobarTypePasswordUpdate = 2,
  // Message Infobar for Saving a Credit Card.
  kInfobarTypeSaveCard = 3,
  // Message Infobar for Translating a page.
  kInfobarTypeTranslate = 4,
  // Message Infobar for Saving an address profile.
  kInfobarTypeSaveAutofillAddressProfile = 5,
  // 6 was used for a Reading List Message, do not use.
  // Message Infobar for media permissions.
  kInfobarTypePermissions = 7,
  // Message Infobar for Tailored Security Service.
  kInfobarTypeTailoredSecurityService = 8,
  // Message Infobar for Sync Error.
  kInfobarTypeSyncError = 9,
  // 10 was used for a Tab Pickup, do not use.
  // Message Infobar for Parcel Tracking.
  kInfobarTypeParcelTracking = 11,
  // Message Infobar for Enhanced Safe Browsing.
  kInfobarTypeEnhancedSafeBrowsing = 12
};

// Message "Confirm Infobars" types, these are the generic kInfobarTypeConfirm
// infobars. Only kInfobarTypeConfirm which want to record unique metrics will
// be listed here. Since these are used for metrics, entries should not be
// renumbered and numeric values should never be reused.
enum class InfobarConfirmType {
  // Confirm Infobar for enabling to "Restore Tabs" after a crash.
  kInfobarConfirmTypeRestore = 0,
  // Confirm Infobar for blocking popups.
  kInfobarConfirmTypeBlockPopups = 1,
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_INFOBAR_TYPE_H_
