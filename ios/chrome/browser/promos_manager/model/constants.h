// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_CONSTANTS_H_

#include <optional>
#include <string_view>

#import "base/values.h"

namespace promos_manager {

// Dictionary key for `promo` identifier in stored impression (base::Value).
extern const char kImpressionPromoKey[];

// Dictionary key for `day` in stored impression (base::Value).
extern const char kImpressionDayKey[];

// Dictionary key for `feature_engagement_migration_completed` stored impression
// (base::Value).
extern const char kImpressionFeatureEngagementMigrationCompletedKey[];

// When a new promo is added, if it's a standard promo, consider adding it to
// `PromosManagerFeatureEngagementTest`.
// LINT.IfChange
enum class Promo {
  Test = 0,            // Test promo used for testing purposes (e.g. unit tests)
  DefaultBrowser = 1,  // Fullscreen Default Browser Promo
  AppStoreRating = 2,  // App Store Rating Prompt
  CredentialProviderExtension = 3,  // Credential Provider Extension
  PostRestoreSignInFullscreen =
      4,  // Post Restore Sign-In (fullscreen, FRE-like promo)
  PostRestoreSignInAlert = 5,  // Post Restore Sign-In (native iOS alert)
  WhatsNew = 6,                // What's New Promo
  // Choice = 7,               // Obsolete. Offer a choice.
  PostRestoreDefaultBrowserAlert =
      8,  // Post Restore Default Browser (native iOS alert)
  DefaultBrowserRemindMeLater = 9,  // Remind me later for default browser.
  // OmniboxPosition = 10,          // Obsolete. Choose between top and bottom
  // omnibox.
  DockingPromo = 11,               // Docking Promo.
  DockingPromoRemindMeLater = 12,  // Docking Promo (Remind Me Later version).
  AllTabsDefaultBrowser = 13,      // "All Tabs" default browser promo.
  MadeForIOSDefaultBrowser = 14,   // "Made For iOS" default browser promo.
  StaySafeDefaultBrowser = 15,     // "Stay Safe" default browser promo.
  PostDefaultAbandonment = 16,     // Post-default browser abandonment alert.
  kMaxValue = PostDefaultAbandonment,
};
// LINT.ThenChange(/ios/chrome/browser/promos_manager/model/constants.cc)
// Also update IOSPromosManagerPromo in
// (/tools/metrics/histograms/metadata/ios/enums.xml).

// Enum for IOS.PromosManager.Promo.Type histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSPromosManagerPromoType {
  kStandardPromoViewProvider = 0,
  kBanneredPromoViewProvider = 1,
  kStandardPromoAlertProvider = 2,
  kStandardPromoDisplayHandler = 3,
  kMaxValue = kStandardPromoDisplayHandler,
};

struct Impression {
  Promo promo;
  // A day (int) is represented as the number of days since the Unix epoch
  // (running from UTC midnight to UTC midnight).
  int day;
  bool feature_engagement_migration_completed;

  Impression(Promo promo, int day, bool feature_engagement_migration_completed)
      : promo(promo),
        day(day),
        feature_engagement_migration_completed(
            feature_engagement_migration_completed) {}
};

// Returns string representation of promos_manager::Promo `promo`.
std::string NameForPromo(Promo promo);

// Returns a string representation of the short name for the provided `promo`.
std::string_view ShortNameForPromo(Promo promo);

// Returns promos_manager::Promo for string `promo`.
std::optional<Promo> PromoForName(std::string_view promo);

std::optional<Impression> ImpressionFromDict(const base::Value::Dict& dict);

}  // namespace promos_manager

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_CONSTANTS_H_
