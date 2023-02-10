// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/ios_chrome_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"

namespace {
// iOS specific list of syncable preferences.
constexpr auto kIOSChromeSyncablePrefsAllowlist =
    base::MakeFixedFlatSet<base::StringPiece>({"dummy"});
}  // namespace

bool IOSChromeSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return kIOSChromeSyncablePrefsAllowlist.count(pref_name) ||
         // Also check if `pref_name` is part of the common set of syncable
         // preferences.
         common_syncable_prefs_database_.IsPreferenceSyncable(pref_name);
}
