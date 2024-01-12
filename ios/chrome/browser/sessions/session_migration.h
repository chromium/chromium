// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_

#include <string>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace ios::sessions {

// Status of the storage migration.
enum class MigrationStatus {
  kSuccess,
  kFailure,
};

// Migrates all sessions found in `paths` from legacy to optimized format
// and returns the status of the migration.
//
// If the migration was a success, all storage is in optimized format and
// all legacy data has been deleted. Otherwise, the original data is left
// untouched and the partially migrated data deleted.
MigrationStatus MigrateSessionsInPathsToOptimized(
    const std::vector<base::FilePath>& paths);

// Migrates all sessions found in `paths` from optimized to legacy format
// and returns the status of the migration.
//
// If the migration was a success, all storage is in optimized format and
// all optimized data has been deleted. Otherwise, the original data is
// left untouched and the partially migrated data deleted.
MigrationStatus MigrateSessionsInPathsToLegacy(
    const std::vector<base::FilePath>& paths);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_MIGRATION_H_
