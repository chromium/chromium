// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/promos_manager/constants.h"

#include "base/notreached.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

namespace promos_manager {

const std::string kImpressionPromoKey = "promo";
const std::string kImpressionDayKey = "day";
const int kNumDaysImpressionHistoryStored = 365;
const std::string kPromoStringifyPrefix = "promos_manager::Promo::";

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
absl::optional<Promo> PromoForName(std::string promo) {
  if (promo == "promos_manager::Promo::Test")
    return promos_manager::Promo::Test;

  if (promo == "promos_manager::Promo::DefaultBrowser")
    return promos_manager::Promo::DefaultBrowser;

  if (promo == "promos_manager::Promo::AppStoreRating")
    return promos_manager::Promo::AppStoreRating;

  if (promo == "promos_manager::Promo::CredentialProviderExtension")
    return promos_manager::Promo::CredentialProviderExtension;

  if (promo == "promos_manager::Promo::PostRestoreSignInFullscreen")
    return promos_manager::Promo::PostRestoreSignInFullscreen;

  if (promo == "promos_manager::Promo::PostRestoreSignInAlert")
    return promos_manager::Promo::PostRestoreSignInAlert;

  if (promo == "promos_manager::Promo::WhatsNew")
    return promos_manager::Promo::WhatsNew;

  return absl::nullopt;
}

std::string ShortNameForPromo(Promo promo) {
  switch (promo) {
    case promos_manager::Promo::Test:
      return "Test";
    case promos_manager::Promo::DefaultBrowser:
      return "DefaultBrowser";
    case promos_manager::Promo::AppStoreRating:
      return "AppStoreRating";
    case promos_manager::Promo::CredentialProviderExtension:
      return "CredentialProviderExtension";
    case promos_manager::Promo::PostRestoreSignInFullscreen:
      return "PostRestoreSignInFullscreen";
    case promos_manager::Promo::PostRestoreSignInAlert:
      return "PostRestoreSignInAlert";
    case promos_manager::Promo::WhatsNew:
      return "WhatsNew";
  }
}

std::string NameForPromo(Promo promo) {
  return kPromoStringifyPrefix + ShortNameForPromo(promo);
}

}  // namespace promos_manager
