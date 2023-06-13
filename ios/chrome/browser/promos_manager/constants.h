// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_

#include "base/strings/string_piece.h"
#import "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace promos_manager {

// Dictionary key for `promo` identifier in stored impression (base::Value).
extern const char kImpressionPromoKey[];

// Dictionary key for `day` in stored impression (base::Value).
extern const char kImpressionDayKey[];

// Dictionary key for `feature_engagement_migration_completed` stored impression
// (base::Value).
extern const char kImpressionFeatureEngagementMigrationCompletedKey[];

// The max number of days for impression history to be stored & maintained.
extern const int kNumDaysImpressionHistoryStored;

enum class Promo {
  Test = 0,            // Test promo used for testing purposes (e.g. unit tests)
  DefaultBrowser = 1,  // Fullscreen Default Browser Promo
  AppStoreRating = 2,  // App Store Rating Prompt
  CredentialProviderExtension = 3,  // Credential Provider Extension
  PostRestoreSignInFullscreen =
      4,  // Post Restore Sign-In (fullscreen, FRE-like promo)
  PostRestoreSignInAlert = 5,  // Post Restore Sign-In (native iOS alert)
  WhatsNew = 6,                // What's New Promo
  kMaxValue = WhatsNew,
};

// Enum for IOS.PromosManager.Promo.ImpressionLimitEvaluation histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSPromosManagerPromoImpressionLimitEvaluationType {
  kValid = 0,
  kInvalidPromoSpecificImpressionLimitTriggered = 1,
  kInvalidPromoAgnosticImpressionLimitTriggered = 2,
  kInvalidGlobalImpressionLimitTriggered = 3,
  kMaxValue = kInvalidGlobalImpressionLimitTriggered,
};

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
base::StringPiece ShortNameForPromo(Promo promo);

// Returns promos_manager::Promo for string `promo`.
absl::optional<Promo> PromoForName(base::StringPiece promo);

absl::optional<Impression> ImpressionFromDict(const base::Value::Dict& dict);

}  // namespace promos_manager

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_CONSTANTS_H_
