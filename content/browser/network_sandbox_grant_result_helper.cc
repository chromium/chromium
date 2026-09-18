// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_sandbox_grant_result_helper.h"

#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/network_sandbox_grant_result.h"
#include "services/network/public/cpp/transferable_directory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {

// A utility function to make it clear what behavior is expected by the network
// context instance depending on the various errors that can happen during data
// migration.
//
// If this function returns 'true' then the `data_directory` should be used (if
// specified in the network context params). If this function returns 'false'
// then the `unsandboxed_data_path` should be used.
bool IsSafeToUseDataPath(SandboxGrantResult result) {
  switch (result) {
    case SandboxGrantResult::kSuccess:
      // A migration occurred, and it was successful.
      return true;
    case SandboxGrantResult::kFailedToGrantSandboxAccessToCache:
    case SandboxGrantResult::kFailedToCreateCacheDirectory:
      // A failure to grant create or grant access to the cache dir does not
      // affect the providence of the data contained in `data_directory` as the
      // migration could have still occurred.
      //
      // These cases are handled internally and so this case should never be
      // hit. It is undefined behavior to proceed in this case so CHECK here.
      base::ImmediateCrash();
    case SandboxGrantResult::kFailedToCreateDataDirectory:
      // A failure to create the `data_directory` is fatal, and the
      // `unsandboxed_data_path` should be used.
      return false;
    case SandboxGrantResult::kFailedToCopyData:
      // A failure to copy the data from `unsandboxed_data_path` to the
      // `data_directory` is fatal, and the `unsandboxed_data_path` should be
      // used.
      return false;
    case SandboxGrantResult::kFailedToDeleteOldData:
      // This is not fatal, as the new data has been correctly migrated, and the
      // deletion will be retried at a later time.
      return true;
    case SandboxGrantResult::kFailedToGrantSandboxAccessToData:
      // If the sandbox could not be granted access to the new data dir, then
      // don't attempt to migrate. This means that the old
      // `unsandboxed_data_path` should be used.
      return false;
    case SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess:
      // No migration was attempted either because of platform constraints or
      // because the network context had no valid data paths (e.g. in-memory or
      // incognito), or `unsandboxed_data_path` was not specified.
      // `data_directory` should be used in this case (if present).
      return true;
    case SandboxGrantResult::kFailedToCreateCheckpointFile:
      // This is fatal, as a failure to create the checkpoint file means that
      // the next time the same network context is used, the data in
      // `unsandboxed_data_path` will be re-copied to the new `data_directory`
      // and thus any changes to the data will be discarded. So in this case,
      // `unsandboxed_data_path` should be used.
      return false;
    case SandboxGrantResult::kNoMigrationRequested:
      // The caller supplied an `unsandboxed_data_path` but did not trigger a
      // migration so the data should be read from the `unsandboxed_data_path`.
      return false;
    case SandboxGrantResult::kMigrationAlreadySucceeded:
      // Migration has already taken place, so `data_directory` contains the
      // valid data.
      return true;
    case SandboxGrantResult::kMigrationAlreadySucceededWithNoAccess:
      // If the sandbox could not be granted access to the new data dir, but the
      // migration has already happened to `data_directory`. This means that the
      // sandbox might not have access to the data but `data_directory` should
      // still be used because it's been migrated.
      return true;
  }
}

}  // namespace

void ProcessSandboxGrantResult(network::mojom::NetworkContextParams& params,
                               SandboxGrantResult grant_access_result) {
  // These two histograms are logged from elsewhere, so don't log them twice.
  CHECK(
      grant_access_result != SandboxGrantResult::kFailedToCreateCacheDirectory,
      base::NotFatalUntil::M160);
  CHECK(grant_access_result !=
            SandboxGrantResult::kFailedToGrantSandboxAccessToCache,
        base::NotFatalUntil::M160);
  base::UmaHistogramEnumeration("NetworkService.GrantSandboxResult",
                                grant_access_result);

  if (grant_access_result != SandboxGrantResult::kSuccess &&
      grant_access_result !=
          SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess &&
      grant_access_result != SandboxGrantResult::kNoMigrationRequested &&
      grant_access_result != SandboxGrantResult::kMigrationAlreadySucceeded) {
    PLOG(ERROR) << "Encountered error while migrating network context data or "
                   "granting sandbox access for "
                << (params.file_paths ? params.file_paths->data_directory.path()
                                      : base::FilePath())
                << ". Result: " << static_cast<int>(grant_access_result);
  }

  if (!IsSafeToUseDataPath(grant_access_result)) {
    // Unsafe to use new `data_directory`. This means that a migration was
    // attempted, and `unsandboxed_data_path` contains the still-valid set of
    // data. Swap the parameters to instruct the network service to use this
    // path for the network context. This of course will mean that if the
    // network service is running sandboxed then this data might not be
    // accessible, but does provide a pathway to user recovery, as the sandbox
    // can just be disabled in this case.
    CHECK(params.file_paths->unsandboxed_data_path.has_value(),
          base::NotFatalUntil::M160);
    params.file_paths->data_directory =
        *params.file_paths->unsandboxed_data_path;
  }

  if (network::TransferableDirectory::IsOpenForTransferRequired()) {
    if (params.file_paths) {
      if (params.file_paths->http_cache_directory) {
        params.file_paths->http_cache_directory->OpenForTransfer();
      }
      if (params.file_paths->shared_dictionary_directory) {
        params.file_paths->shared_dictionary_directory->OpenForTransfer();
      }
      params.file_paths->data_directory.OpenForTransfer();
    }
  }
}

}  // namespace content
