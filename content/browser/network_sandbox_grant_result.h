// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SANDBOX_GRANT_RESULT_H_
#define CONTENT_BROWSER_NETWORK_SANDBOX_GRANT_RESULT_H_

namespace content {

// The outcome of attempting to allow the sandbox access to network context data
// files.
//
// These values are persisted to logs as NetworkServiceSandboxGrantResult and
// should not be renumbered and numeric values should never be reused.
enum class SandboxGrantResult {
  // A migration was requested and was successful.
  kSuccess = 0,
  // Failed to create the new cache directory if it did not already exist.
  kFailedToCreateCacheDirectory = 1,
  // Failed to create the new data directory if it did not already exist.
  kFailedToCreateDataDirectory = 2,
  // Failed to copy a data file from the `unsandboxed_data_path` to the
  // `data_directory` during a migration operation.
  kFailedToCopyData = 3,
  // Failed to delete a data file from the `unsandboxed_data_path` after
  // successfully moving it to the `data_directory` during a migration
  // operation.
  kFailedToDeleteOldData = 4,
  // Failed to grant the sandbox access to the `http_cache_directory`.
  kFailedToGrantSandboxAccessToCache = 5,
  // Failed to grant the sandbox access to the `data_directory` so
  // `unsandboxed_data_path` should be used.
  kFailedToGrantSandboxAccessToData = 6,
  // No migration was attempted either because of platform constraints or
  // because the network context had no valid data paths (e.g. in-memory or
  // incognito), or `unsandboxed_data_path` was not specified.
  kDidNotAttemptToGrantSandboxAccess = 7,
  // Failed to create the checkpoint file that indicates that the files in
  // `data_directory` are valid.
  kFailedToCreateCheckpointFile = 8,
  // No migration was performed because the caller did not set
  // `trigger_migration`. The `unsandboxed_data_path` should be used.
  kNoMigrationRequested = 9,
  // The migration has already completed on a previous load of this network
  // context.
  kMigrationAlreadySucceeded = 10,
  // The migration has already completed on a previous load of this network
  // context but it was not possible to grant the sandbox access to the data.
  kMigrationAlreadySucceededWithNoAccess = 11,
  kMaxValue = kMigrationAlreadySucceededWithNoAccess,
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SANDBOX_GRANT_RESULT_H_
