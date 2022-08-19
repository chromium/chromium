// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_

#include <string>

namespace promos_manager {

// Sentinel value returned from PromosManager::LastSeenDay() if the
// promos_manager::Promo `promo` isn't found in the impressions list.
extern const int kLastSeenDayPromoNotFound;

enum class Promo {
  Test = 0,  // Test promo used for testing purposes (e.g. unit tests)
};

typedef struct Impression {
  Promo promo;
  // A day (int) is represented as the number of days since the Unix epoch
  // (running from UTC midnight to UTC midnight).
  int day;

  Impression(Promo promo, int day) : promo(promo), day(day) {}
} Impression;

// Returns string representation of promos_manager::Promo `promo`.
std::string NameForPromo(Promo promo);

// Returns promos_manager::Promo for string `promo`.
Promo PromoForName(std::string promo);

}  // namespace promos_manager

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
