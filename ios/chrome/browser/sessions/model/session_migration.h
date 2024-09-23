// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_MIGRATION_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_MIGRATION_H_

#include <ostream>
#include <string>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace ios::sessions {

// Result of the storage migration.
struct MigrationResult {
  // Represents the possible migration values.
  enum class Status {
    kSuccess,
    kFailure,
  };

  Status status;
  int32_t next_session_identifier;

  static MigrationResult Success(int32_t next_session_identifier) {
    return {
        .status = Status::kSuccess,
        .next_session_identifier = next_session_identifier,
    };
  }

  static MigrationResult Failure() {
    return {
        .status = Status::kFailure,
        .next_session_identifier = 0,
    };
  }
};

// Migrates all sessions found in `paths` from legacy to optimized format
// and returns the status of the migration.
//
// If the migration was a success, all storage is in optimized format and
// all legacy data has been deleted. Otherwise, the original data is left
// untouched and the partially migrated data deleted.
MigrationResult MigrateSessionsInPathsToOptimized(
    const std::vector<base::FilePath>& paths,
    int32_t next_session_identifier);

// Migrates all sessions found in `paths` from optimized to legacy format
// and returns the status of the migration.
//
// If the migration was a success, all storage is in legacy format and all
// optimized data has been deleted. Otherwise, the original data is left
// untouched and the partially migrated data deleted.
MigrationResult MigrateSessionsInPathsToLegacy(
    const std::vector<base::FilePath>& paths,
    int32_t next_session_identifier);

// Comparison operators for testing.
bool operator==(const MigrationResult& lhs, const MigrationResult& rhs);
bool operator!=(const MigrationResult& lhs, const MigrationResult& rhs);

// Insertion operator for testing.
std::ostream& operator<<(std::ostream& stream, const MigrationResult& result);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_MIGRATION_H_
