// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/prefs/ios_chrome_syncable_prefs_database.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "components/handoff/pref_names_ios.h"
#include "ios/chrome/browser/prefs/pref_names.h"
#include "ios/chrome/browser/voice/voice_search_prefs.h"

namespace browser_sync {

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding a new entry, append the
// enumerator to the end. When removing an unused enumerator, comment it out,
// making it clear the value was previously used.
// Please also add new entries to `SyncablePref` enum in
// tools/metrics/histograms/enums.xml.
enum {
  // Starts with 200000 to avoid clash with prefs listed in
  // common_syncable_prefs_database.cc and
  // chrome_syncable_prefs_database.cc.
  kArticlesForYouEnabled = 200000,
  kContextualSearchEnabled = 200001,
  kDefaultCharset = 200002,
  kEnableDoNotTrack = 200003,
  kIosHandoffToOtherDevices = 200004,
  kNetworkPredictionSetting = 200005,
  kNTPContentSuggestionsEnabled = 200006,
  kNTPContentSuggestionsForSupervisedUserEnabled = 200007,
  kSearchSuggestEnabled = 200008,
  kTrackPricesOnTabsEnabled = 200009,
  kVoiceSearchLocale = 200010
};
}  // namespace syncable_prefs_ids

const auto& SyncablePreferences() {
  // iOS specific list of syncable preferences.
  static const auto kIOSChromeSyncablePrefsAllowlist = base::MakeFixedFlatMap<
      base::StringPiece, sync_preferences::SyncablePrefMetadata>(
      {{prefs::kArticlesForYouEnabled,
        {syncable_prefs_ids::kArticlesForYouEnabled, syncer::PREFERENCES}},
       {prefs::kContextualSearchEnabled,
        {syncable_prefs_ids::kContextualSearchEnabled, syncer::PREFERENCES}},
       {prefs::kDefaultCharset,
        {syncable_prefs_ids::kDefaultCharset, syncer::PREFERENCES}},
       {prefs::kEnableDoNotTrack,
        {syncable_prefs_ids::kEnableDoNotTrack, syncer::PREFERENCES}},
       {prefs::kIosHandoffToOtherDevices,
        {syncable_prefs_ids::kIosHandoffToOtherDevices, syncer::PREFERENCES}},
       {prefs::kNetworkPredictionSetting,
        {syncable_prefs_ids::kNetworkPredictionSetting, syncer::PREFERENCES}},
       {prefs::kNTPContentSuggestionsEnabled,
        {syncable_prefs_ids::kNTPContentSuggestionsEnabled,
         syncer::PREFERENCES}},
       {prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
        {syncable_prefs_ids::kNTPContentSuggestionsForSupervisedUserEnabled,
         syncer::PREFERENCES}},
       {prefs::kSearchSuggestEnabled,
        {syncable_prefs_ids::kSearchSuggestEnabled, syncer::PREFERENCES}},
       {prefs::kTrackPricesOnTabsEnabled,
        {syncable_prefs_ids::kTrackPricesOnTabsEnabled, syncer::PREFERENCES}},
       {prefs::kVoiceSearchLocale,
        {syncable_prefs_ids::kVoiceSearchLocale, syncer::PREFERENCES}}});
  return kIOSChromeSyncablePrefsAllowlist;
}
}  // namespace

absl::optional<sync_preferences::SyncablePrefMetadata>
IOSChromeSyncablePrefsDatabase::GetSyncablePrefMetadata(
    const std::string& pref_name) const {
  const auto* it = SyncablePreferences().find(pref_name);
  if (it != SyncablePreferences().end()) {
    DCHECK(!common_syncable_prefs_database_.GetSyncablePrefMetadata(pref_name)
                .has_value());
    return it->second;
  }
  // Check in `common_syncable_prefs_database_`.
  return common_syncable_prefs_database_.GetSyncablePrefMetadata(pref_name);
}
}  // namespace browser_sync
