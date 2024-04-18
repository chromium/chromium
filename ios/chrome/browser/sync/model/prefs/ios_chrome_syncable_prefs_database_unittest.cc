// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/prefs/ios_chrome_syncable_prefs_database.h"

#include <string_view>

#include "base/test/metrics/histogram_enum_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(IOSChromeSyncablePrefsDatabaseTest, CheckMetricsEnum) {
  std::optional<base::HistogramEnumEntryMap> syncable_pref_enums =
      base::ReadEnumFromEnumsXml("SyncablePref", "sync");

  ASSERT_TRUE(syncable_pref_enums.has_value())
      << "Failed to read SyncablePref enum from "
         "tools/metrics/histograms/metadata/sync/enums.xml.";

  browser_sync::IOSChromeSyncablePrefsDatabase db;
  std::map<std::string_view, sync_preferences::SyncablePrefMetadata>
      syncable_prefs = db.GetAllSyncablePrefsForTest();
  for (const auto& [pref_name, metadata] : syncable_prefs) {
    EXPECT_TRUE(syncable_pref_enums->contains(metadata.syncable_pref_id()))
        << "Enum entry for preference " << pref_name
        << " syncable_pref_id=" << metadata.syncable_pref_id()
        << " not found in enums.xml.";
  }
}

}  // namespace
