// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/promos_manager/constants.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

namespace promos_manager {

const char kImpressionPromoKey[] = "promo";
const char kImpressionDayKey[] = "day";
const char kImpressionFeatureEngagementMigrationCompletedKey[] =
    "feature_engagement_migration_completed";
const int kNumDaysImpressionHistoryStored = 365;

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
absl::optional<Promo> PromoForName(base::StringPiece promo) {
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

  if (promo == "promos_manager::Promo::Choice") {
    return promos_manager::Promo::Choice;
  }

  if (promo == "promos_manager::Promo::PostRestoreDefaultBrowserAlert") {
    return promos_manager::Promo::PostRestoreDefaultBrowserAlert;
  }

  return absl::nullopt;
}

std::string NameForPromo(Promo promo) {
  return base::StrCat({"promos_manager::Promo::", ShortNameForPromo(promo)});
}

base::StringPiece ShortNameForPromo(Promo promo) {
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
    case promos_manager::Promo::Choice:
      return "Choice";
    case promos_manager::Promo::PostRestoreDefaultBrowserAlert:
      return "PostRestoreDefaultBrowserAlert";
  }
}

absl::optional<promos_manager::Impression> ImpressionFromDict(
    const base::Value::Dict& dict) {
  const std::string* stored_promo =
      dict.FindString(promos_manager::kImpressionPromoKey);
  absl::optional<int> stored_day =
      dict.FindInt(promos_manager::kImpressionDayKey);
  absl::optional<bool> stored_migration_complete = dict.FindBool(
      promos_manager::kImpressionFeatureEngagementMigrationCompletedKey);

  // Skip malformed impression history. (This should almost never happen.)
  if (!stored_promo || !stored_day.has_value()) {
    return absl::nullopt;
  }

  absl::optional<promos_manager::Promo> promo =
      promos_manager::PromoForName(*stored_promo);

  // Skip malformed impression history. (This should almost never happen.)
  if (!promo.has_value()) {
    return absl::nullopt;
  }

  return promos_manager::Impression(promo.value(), stored_day.value(),
                                    stored_migration_complete.value_or(false));
}

}  // namespace promos_manager
