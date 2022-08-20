// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

#import <Foundation/Foundation.h>
#import <vector>

#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"

class PromosManagerTest;

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager.
class PromosManager {
 public:
  explicit PromosManager(PrefService* local_state);
  ~PromosManager();

  // Initialize the Promos Manager by restoring state from Prefs. Must be called
  // after creation and before any other operation.
  void Init();

 private:
  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // base::Value::List of active promos.
  base::Value::List active_promos_;

  // base::Value::List of the promo impression history.
  base::Value::List impression_history_;

  // Impression limits that count against all promos.
  NSArray<ImpressionLimit*>* GlobalImpressionLimits();

  // Impression limits that count against any given promo.
  NSArray<ImpressionLimit*>* GlobalPerPromoImpressionLimits();

  // Returns the most recent day (int) that `promo` was seen by the user.
  //
  // A day (int) is represented as the number of days since the Unix epoch
  // (running from UTC midnight to UTC midnight).
  //
  // Assumes that `sorted_impressions` is sorted by day (most recent -> least
  // recent).
  //
  // Returns promos_manager::kLastSeenDayPromoNotFound if `promo` isn't
  // found in the impressions list.
  int LastSeenDay(
      promos_manager::Promo promo,
      std::vector<promos_manager::Impression>& sorted_impressions) const;

  // Allow unit tests to access private methods.
  friend class PromosManagerTest;
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest, ReturnsLastSeenDayForPromo);
  FRIEND_TEST_ALL_PREFIXES(PromosManagerTest,
                           ReturnsSentinelForNonExistentPromo);
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
