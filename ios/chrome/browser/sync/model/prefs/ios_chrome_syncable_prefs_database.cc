// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/prefs/ios_chrome_syncable_prefs_database.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/handoff/pref_names_ios.h"
#include "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "ios/chrome/browser/voice/model/voice_search_prefs.h"

namespace browser_sync {

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding a new entry, append the
// enumerator to the end and add it to the `SyncablePref` enum in
// tools/metrics/histograms/enums.xml. When removing an unused enumerator,
// comment it out here, making it clear the value was previously used, and
// add "(obsolete)" to the corresponding entry in enums.xml.
// LINT.IfChange(IosSyncablePref)
enum {
  // Starts with 200000 to avoid clash with prefs listed in
  // common_syncable_prefs_database.cc and
  // chrome_syncable_prefs_database.cc.
  kArticlesForYouEnabled = 200000,
  // kContextualSearchEnabled = 200001,  // deprecated
  kDefaultCharset = 200002,
  kEnableDoNotTrack = 200003,
  kIosHandoffToOtherDevices = 200004,
  kNetworkPredictionSetting = 200005,
  kNTPContentSuggestionsEnabled = 200006,
  kNTPContentSuggestionsForSupervisedUserEnabled = 200007,
  kSearchSuggestEnabled = 200008,
  kTrackPricesOnTabsEnabled = 200009,
  kVoiceSearchLocale = 200010
  // See components/sync_preferences/README.md about adding new entries here.
  // vvvvv IMPORTANT! vvvvv
  // Note to the reviewer: IT IS YOUR RESPONSIBILITY to ensure that new syncable
  // prefs follow privacy guidelines! See the readme file linked above for
  // guidance and escalation path in case anything is unclear.
  // ^^^^^ IMPORTANT! ^^^^^
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:IosSyncablePref)
}  // namespace syncable_prefs_ids

// iOS specific list of syncable preferences.
constexpr auto kIOSChromeSyncablePrefsAllowlist =
    base::MakeFixedFlatMap<std::string_view,
                           sync_preferences::SyncablePrefMetadata>({
        {prefs::kArticlesForYouEnabled,
         {syncable_prefs_ids::kArticlesForYouEnabled, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kDefaultCharset,
         {syncable_prefs_ids::kDefaultCharset, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kEnableDoNotTrackIos,
         {syncable_prefs_ids::kEnableDoNotTrack, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kIosHandoffToOtherDevices,
         {syncable_prefs_ids::kIosHandoffToOtherDevices, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kNetworkPredictionSetting,
         {syncable_prefs_ids::kNetworkPredictionSetting, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kNTPContentSuggestionsEnabled,
         {syncable_prefs_ids::kNTPContentSuggestionsEnabled,
          syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
         {syncable_prefs_ids::kNTPContentSuggestionsForSupervisedUserEnabled,
          syncer::PREFERENCES, sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kSearchSuggestEnabled,
         {syncable_prefs_ids::kSearchSuggestEnabled, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kTrackPricesOnTabsEnabled,
         {syncable_prefs_ids::kTrackPricesOnTabsEnabled, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
        {prefs::kVoiceSearchLocale,
         {syncable_prefs_ids::kVoiceSearchLocale, syncer::PREFERENCES,
          sync_preferences::PrefSensitivity::kNone,
          sync_preferences::MergeBehavior::kNone}},
    });

}  // namespace

std::optional<sync_preferences::SyncablePrefMetadata>
IOSChromeSyncablePrefsDatabase::GetSyncablePrefMetadata(
    std::string_view pref_name) const {
  const auto it = kIOSChromeSyncablePrefsAllowlist.find(pref_name);
  if (it != kIOSChromeSyncablePrefsAllowlist.end()) {
    DCHECK(!common_syncable_prefs_database_.GetSyncablePrefMetadata(pref_name)
                .has_value());
    return it->second;
  }
  // Check in `common_syncable_prefs_database_`.
  return common_syncable_prefs_database_.GetSyncablePrefMetadata(pref_name);
}

std::map<std::string_view, sync_preferences::SyncablePrefMetadata>
IOSChromeSyncablePrefsDatabase::GetAllSyncablePrefsForTest() const {
  std::map<std::string_view, sync_preferences::SyncablePrefMetadata>
      syncable_prefs;
  base::ranges::copy(kIOSChromeSyncablePrefsAllowlist,
                     std::inserter(syncable_prefs, syncable_prefs.end()));
  base::ranges::move(
      common_syncable_prefs_database_.GetAllSyncablePrefsForTest(),  // IN-TEST
      std::inserter(syncable_prefs, syncable_prefs.end()));
  return syncable_prefs;
}
}  // namespace browser_sync
