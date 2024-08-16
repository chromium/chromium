// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/promos_manager/model/constants.h"

#include <optional>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace promos_manager {

const char kImpressionPromoKey[] = "promo";
const char kImpressionDayKey[] = "day";
const char kImpressionFeatureEngagementMigrationCompletedKey[] =
    "feature_engagement_migration_completed";

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
std::optional<Promo> PromoForName(std::string_view promo) {
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

  if (promo == "promos_manager::Promo::PostRestoreDefaultBrowserAlert") {
    return promos_manager::Promo::PostRestoreDefaultBrowserAlert;
  }

  if (promo == "promos_manager::Promo::DefaultBrowserRemindMeLater") {
    return promos_manager::Promo::DefaultBrowserRemindMeLater;
  }

  if (promo == "promos_manager::Promo::DockingPromo") {
    return promos_manager::Promo::DockingPromo;
  }

  if (promo == "promos_manager::Promo::DockingPromoRemindMeLater") {
    return promos_manager::Promo::DockingPromoRemindMeLater;
  }

  if (promo == "promos_manager::Promo::AllTabsDefaultBrowser") {
    return promos_manager::Promo::AllTabsDefaultBrowser;
  }

  if (promo == "promos_manager::Promo::MadeForIOSDefaultBrowser") {
    return promos_manager::Promo::MadeForIOSDefaultBrowser;
  }

  if (promo == "promos_manager::Promo::StaySafeDefaultBrowser") {
    return promos_manager::Promo::StaySafeDefaultBrowser;
  }

  if (promo == "promos_manager::Promo::PostDefaultAbandonment") {
    return promos_manager::Promo::PostDefaultAbandonment;
  }

  return std::nullopt;
}

std::string NameForPromo(Promo promo) {
  return base::StrCat({"promos_manager::Promo::", ShortNameForPromo(promo)});
}

std::string_view ShortNameForPromo(Promo promo) {
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
    case promos_manager::Promo::PostRestoreDefaultBrowserAlert:
      return "PostRestoreDefaultBrowserAlert";
    case promos_manager::Promo::DefaultBrowserRemindMeLater:
      return "DefaultBrowserRemindMeLater";
    case promos_manager::Promo::DockingPromo:
      return "DockingPromo";
    case promos_manager::Promo::DockingPromoRemindMeLater:
      return "DockingPromoRemindMeLater";
    case promos_manager::Promo::AllTabsDefaultBrowser:
      return "AllTabsDefaultBrowser";
    case promos_manager::Promo::MadeForIOSDefaultBrowser:
      return "MadeForIOSDefaultBrowser";
    case promos_manager::Promo::StaySafeDefaultBrowser:
      return "StaySafeDefaultBrowser";
    case promos_manager::Promo::PostDefaultAbandonment:
      return "PostDefaultAbandonment";
  }
}

std::optional<promos_manager::Impression> ImpressionFromDict(
    const base::Value::Dict& dict) {
  const std::string* stored_promo =
      dict.FindString(promos_manager::kImpressionPromoKey);
  std::optional<int> stored_day =
      dict.FindInt(promos_manager::kImpressionDayKey);
  std::optional<bool> stored_migration_complete = dict.FindBool(
      promos_manager::kImpressionFeatureEngagementMigrationCompletedKey);

  // Skip malformed impression history. (This should almost never happen.)
  if (!stored_promo || !stored_day.has_value()) {
    return std::nullopt;
  }

  std::optional<promos_manager::Promo> promo =
      promos_manager::PromoForName(*stored_promo);

  // Skip malformed impression history. (This should almost never happen.)
  if (!promo.has_value()) {
    return std::nullopt;
  }

  return promos_manager::Impression(promo.value(), stored_day.value(),
                                    stored_migration_complete.value_or(false));
}

}  // namespace promos_manager
