// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/prefs/ios_chrome_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"
#include "components/handoff/pref_names_ios.h"
#include "ios/chrome/browser/prefs/pref_names.h"
#include "ios/chrome/browser/voice/voice_search_prefs.h"

namespace browser_sync {

bool IOSChromeSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  // iOS specific list of syncable preferences.
  static const auto kIOSChromeSyncablePrefsAllowlist =
      base::MakeFixedFlatSet<base::StringPiece>(
          {prefs::kArticlesForYouEnabled, prefs::kContextualSearchEnabled,
           prefs::kDefaultCharset, prefs::kEnableDoNotTrack,
           prefs::kIosHandoffToOtherDevices, prefs::kNetworkPredictionSetting,
           prefs::kNTPContentSuggestionsEnabled,
           prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
           prefs::kSearchSuggestEnabled, prefs::kTrackPricesOnTabsEnabled,
           prefs::kVoiceSearchLocale});
  return kIOSChromeSyncablePrefsAllowlist.count(pref_name) ||
         // Also check if `pref_name` is part of the common set of syncable
         // preferences.
         common_syncable_prefs_database_.IsPreferenceSyncable(pref_name);
}
}  // namespace browser_sync
