// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/promos_manager/constants.h"

#include "base/notreached.h"

namespace promos_manager {

const std::string kImpressionPromoKey = "promo";
const std::string kImpressionDayKey = "day";
const int kLastSeenDayPromoNotFound = -1;
const int kNumDaysImpressionHistoryStored = 365;

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
Promo PromoForName(std::string promo) {
  if (promo == "promos_manager::Promo::Test") {
    return promos_manager::Promo::Test;
  } else if (promo == "promos_manager::Promo::DefaultBrowser") {
    return promos_manager::Promo::DefaultBrowser;
  } else if (promo == "promos_manager::Promo::AppStoreRating") {
    return promos_manager::Promo::AppStoreRating;
  } else if (promo == "promos_manager::Promo::CredentialProviderExtension") {
    return promos_manager::Promo::CredentialProviderExtension;
  } else {
    NOTREACHED();

    // Returns promos_manager::Promo::Test by default, but this should never be
    // reached!
    return promos_manager::Promo::Test;
  }
}

std::string NameForPromo(Promo promo) {
  switch (promo) {
    case promos_manager::Promo::Test:
      return "promos_manager::Promo::Test";
    case promos_manager::Promo::DefaultBrowser:
      return "promos_manager::Promo::DefaultBrowser";
    case promos_manager::Promo::AppStoreRating:
      return "promos_manager::Promo::AppStoreRating";
    case promos_manager::Promo::CredentialProviderExtension:
      return "promos_manager::Promo::CredentialProviderExtension";
  }
}

}  // namespace promos_manager
