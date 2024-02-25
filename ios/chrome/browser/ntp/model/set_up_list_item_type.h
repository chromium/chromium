// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_TYPE_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_TYPE_H_

// The possible types of items for the Set Up List. This enum must match the
// UMA histogram enum IOSSetUpListItemType.
//
// LINT.IfChange
enum class SetUpListItemType {
  kSignInSync = 1,
  kDefaultBrowser = 2,
  kAutofill = 3,
  kFollow = 4,
  kAllSet = 5,
  kNotifications = 6,
  kMaxValue = kNotifications
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_ITEM_TYPE_H_
