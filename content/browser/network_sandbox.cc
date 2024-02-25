// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_sandbox.h"

#include "base/dcheck_is_on.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "content/browser/network_sandbox_grant_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_client.h"
#include "sql/database.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "sandbox/policy/features.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

// A filename that represents that the data contained within `data_directory`
// has been migrated successfully and the data in `unsandboxed_data_path` is now
// invalid.
const base::FilePath::CharType kCheckpointFileName[] =
    FILE_PATH_LITERAL("NetworkDataMigrated");

// A platform specific set of parameters that is used when granting the sandbox
// access to the network context data.
struct SandboxParameters {
#if BUILDFLAG(IS_WIN)
  std::wstring lpac_capability_name;
#if DCHECK_IS_ON()
  bool sandbox_enabled;
#endif  // DCHECK_IS_ON()
#endif  // BUILDFLAG(IS_WIN)
};

// Deletes the old data for a data file called `filename` from `old_path`. If
// `file_path` refers to an SQL database then `is_sql` should be set to true,
// and the journal file will also be deleted.
//
// Returns SandboxGrantResult::kSuccess if the all delete operations completed
// successfully. Returns SandboxGrantResult::kFailedToDeleteData if a file could
// not be deleted.
SandboxGrantResult MaybeDeleteOldData(
    const base::FilePath& old_path,
    const std::optional<base::FilePath>& filename,
    bool is_sql) {
  // The path to the specific data file might not have been specified in the
  // network context params. In that case, nothing to delete.
  if (!filename.has_value())
    return SandboxGrantResult::kSuccess;

  // Check old path exists, and is a directory.
  DCHECK(base::DirectoryExists(old_path));

  base::FilePath old_file_path = old_path.Append(*filename);

  SandboxGrantResult last_error = SandboxGrantResult::kSuccess;
  // File might have already been deleted, or simply does not exist yet.
  if (base::PathExists(old_file_path)) {
    if (!base::DeleteFile(old_file_path)) {
      PLOG(ERROR) << "Failed to delete file " << old_file_path;
      // Continue on error.
      last_error = SandboxGrantResult::kFailedToDeleteOldData;
    }
  }

  if (!is_sql)
    return last_error;

  base::FilePath old_journal_path = sql::Database::JournalPath(old_file_path);
  // There might not be a journal file, or it's already been deleted.
  if (!base::PathExists(old_journal_path))
    return last_error;

  if (base::PathExists(old_journal_path)) {
    if (!base::DeleteFile(old_journal_path)) {
      PLOG(ERROR) << "Failed to delete file " << old_journal_path;
      // Continue on error.
      last_error = SandboxGrantResult::kFailedToDeleteOldData;
    }
  }

  return last_error;
}

// Copies data file called `filename` from `old_path` to `new_path` (which must
// both be directories). If `file_path` refers to an SQL database then `is_sql`
// should be set to true, and the journal file will also be migrated.
// Destination files will be overwritten if they exist already.
//
// Returns SandboxGrantResult::kSuccess if the operation completed successfully.
// Returns SandboxGrantResult::kFailedToCopyData if a file could not be copied.
SandboxGrantResult MaybeCopyData(const base::FilePath& old_path,
                                 const base::FilePath& new_path,
                                 const std::optional<base::FilePath>& filename,
                                 bool is_sql) {
  // The path to the specific data file might not have been specified in the
  // network context params. In that case, no files need to be moved.
  if (!filename.has_value())
    return SandboxGrantResult::kSuccess;

  // Check both paths exist, and are directories.
  DCHECK(base::DirectoryExists(old_path) && base::DirectoryExists(new_path));

  base::FilePath old_file_path = old_path.Append(*filename);
  base::FilePath new_file_path = new_path.Append(*filename);

  // Note that this code will overwrite the new file with the old file even if
  // it exists already.
  if (base::PathExists(old_file_path)) {
    // Delete file to make sure that inherited permissions are set on the new
    // file.
    base::DeleteFile(new_file_path);
    if (!base::CopyFile(old_file_path, new_file_path)) {
      PLOG(ERROR) << "Failed to copy file " << old_file_path << " to "
                  << new_file_path;
      // Do not attempt to copy journal file if copy of main database file
      // fails.
      return SandboxGrantResult::kFailedToCopyData;
    }
  }

  if (!is_sql)
    return SandboxGrantResult::kSuccess;

  base::FilePath old_journal_path = sql::Database::JournalPath(old_file_path);
  // There might not be a journal file, or it's already been moved.
  if (!base::PathExists(old_journal_path))
    return SandboxGrantResult::kSuccess;

  base::FilePath new_journal_path = sql::Database::JournalPath(new_file_path);

  // Delete file to make sure that inherited permissions are set on the new
  // file.
  base::DeleteFile(new_journal_path);

  if (!base::CopyFile(old_journal_path, new_journal_path)) {
    PLOG(ERROR) << "Failed to copy file " << old_journal_path << " to "
                << new_journal_path;
    return SandboxGrantResult::kFailedToCopyData;
  }

  return SandboxGrantResult::kSuccess;
}

// Deletes old data from `unsandboxed_data_path` if a migration operation has
// been successful.
SandboxGrantResult CleanUpOldData(
    network::mojom::NetworkContextParams* params) {
  // Never delete old data unless the checkpoint file exists.
  DCHECK(base::PathExists(
      params->file_paths->data_directory.path().Append(kCheckpointFileName)));

  SandboxGrantResult last_error = SandboxGrantResult::kSuccess;
  SandboxGrantResult result = MaybeDeleteOldData(
      *params->file_paths->unsandboxed_data_path,
      params->file_paths->cookie_database_name, /*is_sql=*/true);
  if (result != SandboxGrantResult::kSuccess)
    last_error = result;

  result = MaybeDeleteOldData(
      *params->file_paths->unsandboxed_data_path,
      params->file_paths->http_server_properties_file_name, /*is_sql=*/false);
  if (result != SandboxGrantResult::kSuccess)
    last_error = result;

  result = MaybeDeleteOldData(
      *params->file_paths->unsandboxed_data_path,
      params->file_paths->transport_security_persister_file_name,
      /*is_sql=*/false);
  if (result != SandboxGrantResult::kSuccess)
    last_error = result;

  result = MaybeDeleteOldData(
      *params->file_paths->unsandboxed_data_path,
      params->file_paths->reporting_and_nel_store_database_name,
      /*is_sql=*/true);
  if (result != SandboxGrantResult::kSuccess)
    last_error = result;

  result = MaybeDeleteOldData(*params->file_paths->unsandboxed_data_path,
                              params->file_paths->trust_token_database_name,
                              /*is_sql=*/true);
  if (result != SandboxGrantResult::kSuccess)
    last_error = result;
  return last_error;
}

// Grants the sandbox access to the specified `path`, which must be a directory
// that exists.  On Windows, the LPAC capability name should be supplied in the
// `sandbox_params` to specify the name of the LPAC capability to be applied to
// the path.  On platforms which support directory transfer, the directory is
// opened as a handle which is then sent to the NetworkService.
// Returns true if the sandbox was successfully granted access to the path.
bool MaybeGrantAccessToDataPath(const SandboxParameters& sandbox_params,
                                network::TransferableDirectory* directory) {
  // There is no need to set file permissions if the network service is running
  // in-process.
  if (IsInProcessNetworkService())
    return true;
  // Only do this on directories.
  if (!base::DirectoryExists(directory->path())) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  // On platforms that don't support the LPAC sandbox, do nothing.
  if (!sandbox::policy::features::IsNetworkSandboxSupported()) {
    return true;
  }
  DCHECK(!sandbox_params.lpac_capability_name.empty());
  auto ac_sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox_params.lpac_capability_name});

  // Grant recursive access to directory. This also means new files in the
  // directory will inherit the ACE.
  return base::win::GrantAccessToPath(
      directory->path(), ac_sids,
      GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE, /*recursive=*/true);
#else
  if (directory->IsOpenForTransferRequired()) {
    directory->OpenForTransfer();
    return true;
  }

  return true;
#endif  // BUILDFLAG(IS_WIN)
}

// See the description in the header file.
//
// This process has a few stages:
// 1. Create and grant the sandbox access to the cache dir.
// 2. If `data_directory` is not specified then the caller is using in-memory
// storage and so there's nothing to do. END.
// 3. If `unsandboxed_data_path` is not specified then the caller is not aware
// of the sandbox or migration, and the steps terminate here with
// `data_directory` used by the network context and END.
// 4. If migration has already taken place, regardless of whether it's requested
// this time, grant the sandbox access to the `data_directory` (since this needs
// to be done every time), and terminate here with `data_directory` being used.
// END.
// 5. If migration is not requested, then terminate here with
// `unsandboxed_data_path` being used. END.
// 6. At this point, migration has been requested and hasn't already happened,
// so begin a migration attempt. If any of these steps fail, then bail out, and
// `unsandboxed_data_path` is used.
// 7. Grant the sandbox access to the `data_directory` (this is done before
// copying the files to use inherited ACLs when copying files on Windows).
// 8. Copy all the data files one by one from the `unsandboxed_data_path` to the
// `data_directory`.
// 9. Once all the files have been copied, lay down the Checkpoint file in the
// `data_directory`.
// 10. Delete all the original files (if they exist) from
// `unsandboxed_data_path`.
SandboxGrantResult MaybeGrantSandboxAccessToNetworkContextData(
    const SandboxParameters& sandbox_params,
    network::mojom::NetworkContextParams* params) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#if BUILDFLAG(IS_WIN)
#if DCHECK_IS_ON()
  params->win_permissions_set = true;
#endif
#endif  // BUILDFLAG(IS_WIN)

  // No file paths (e.g. in-memory context) so nothing to do.
  if (!params->file_paths) {
    return SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess;
  }

  // HTTP cache path is special, and not under `data_directory` so must also
  // be granted access. Continue attempting to grant access to the other files
  // if this part fails.
  if (params->file_paths->http_cache_directory && params->http_cache_enabled) {
    // The path must exist for the cache ACL to be set. Create if needed.
    if (base::CreateDirectory(
            params->file_paths->http_cache_directory->path())) {
      // Note, this code always grants access to the cache directory even when
      // the sandbox is not enabled. This is a optimization (on Windows)
      // because by setting the ACL on the directory earlier rather than
      // later, it ensures that any new files created by the cache subsystem
      // get the inherited ACE rather than having to set them manually later.
      SCOPED_UMA_HISTOGRAM_TIMER("NetworkService.TimeToGrantCacheAccess");
      TRACE_EVENT("startup", "NetworkSandbox.MaybeGrantAccessToDataPath");
      if (!MaybeGrantAccessToDataPath(
              sandbox_params, &*params->file_paths->http_cache_directory)) {
        PLOG(ERROR) << "Failed to grant sandbox access to cache directory "
                    << params->file_paths->http_cache_directory->path();
      }
    }
  }
  if (params->file_paths->shared_dictionary_directory &&
      params->shared_dictionary_enabled) {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "NetworkService.TimeToGrantSharedDictionaryAccess");
    // The path must exist for the cache ACL to be set. Create if needed.
    if (base::CreateDirectory(
            params->file_paths->shared_dictionary_directory->path())) {
      if (!MaybeGrantAccessToDataPath(
              sandbox_params,
              &*params->file_paths->shared_dictionary_directory)) {
        PLOG(ERROR) << "Failed to grant sandbox access to shared dictionary "
                       "directory "
                    << params->file_paths->shared_dictionary_directory->path();
      }
    }
  }

  // No data directory, so rest of the files and databases are in memory.
  // Nothing to do.
  if (params->file_paths->data_directory.path().empty()) {
    return SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess;
  }

  if (!params->file_paths->unsandboxed_data_path.has_value()) {
#if BUILDFLAG(IS_WIN) && DCHECK_IS_ON()
    // On Windows, if network sandbox is enabled then there a migration must
    // happen, so a `unsandboxed_data_path` must be specified.
    DCHECK(!sandbox_params.sandbox_enabled);
#endif
    // Trigger migration should never be requested if `unsandboxed_data_path` is
    // not set.
    DCHECK(!params->file_paths->trigger_migration);
    // Nothing to do here if `unsandboxed_data_path` is not specified.
    return SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess;
  }

  // If these paths are ever the same then this is a mistake, as the file
  // permissions will be applied to the top level path which could contain other
  // data that should not be accessible by the network sandbox.
  DCHECK_NE(params->file_paths->data_directory.path(),
            *params->file_paths->unsandboxed_data_path);

  // Four cases need to be handled here.
  //
  // 1. No Checkpoint file, and `trigger_migration` is false: Data is still in
  // `unsandboxed_data_path` and sandbox does not need to be granted access. No
  // migration happens.
  // 2. No Checkpoint file, and `trigger_migration` is true: Data is in
  // `unsandboxed_data_path` and needs to be migrated to `data_directory`, and
  // the sandbox needs to be granted access to `data_directory`.
  // 3. Checkpoint file, and `trigger_migration` is false: Data is in
  // `data_directory` (already migrated) and sandbox needs to be granted access
  // to `data_directory`.
  // 4. Checkpoint file, and `trigger_migration` is true: Data is in
  // `data_directory` (already migrated) and sandbox needs to be granted access
  // to `data_directory`. This is the same as above and `trigger_migration`
  // changes nothing, as it's already happened.
  base::FilePath checkpoint_filename =
      params->file_paths->data_directory.path().Append(kCheckpointFileName);
  bool migration_already_happened = base::PathExists(checkpoint_filename);

  // Case 1. above where nothing is done.
  if (!params->file_paths->trigger_migration && !migration_already_happened) {
#if BUILDFLAG(IS_WIN) && DCHECK_IS_ON()
    // On Windows, if network sandbox is enabled then there a migration must
    // happen, so `trigger_migration` must be true, or a migration must have
    // already happened.
    DCHECK(!sandbox_params.sandbox_enabled);
#endif
    return SandboxGrantResult::kNoMigrationRequested;
  }

  // Create the `data_directory` if necessary so access can be granted to it.
  // Note that if a migration has already happened then this does nothing, as
  // the directory already exists.
  if (!base::CreateDirectory(params->file_paths->data_directory.path())) {
    PLOG(ERROR) << "Failed to create network context data directory "
                << params->file_paths->data_directory.path();
    // This is a fatal error, if the `data_directory` does not exist then
    // migration cannot be attempted. In this case the network context will
    // operate using `unsandboxed_data_path` and the migration attempt will be
    // retried the next time the same network context is created with
    // `trigger_migration` set.
    return SandboxGrantResult::kFailedToCreateDataDirectory;
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("NetworkService.TimeToGrantDataAccess");
    // This must be done on each load of the network context for two
    // platform-specific reasons:
    //
    // 1. On Windows Chrome, the LPAC SID for each channel is different so it is
    // possible that this data might be read by a different channel and we need
    // to explicitly support that.
    // 2. Other platforms such as macOS and Linux need to grant access each time
    // as they do not rely on filesystem permissions, but runtime sandbox broker
    // permissions.
    if (!MaybeGrantAccessToDataPath(sandbox_params,
                                    &params->file_paths->data_directory)) {
      PLOG(ERROR)
          << "Failed to grant sandbox access to network context data directory "
          << params->file_paths->data_directory.path();
      // If migration has already happened there isn't much that can be done
      // about this, the data has already moved, but the sandbox might not have
      // access.
      if (migration_already_happened)
        return SandboxGrantResult::kMigrationAlreadySucceededWithNoAccess;
      // If migration hasn't happened yet, then fail here, and do not attempt to
      // migrate or proceed further. Better to just leave the data where it is.
      // In this case `unsandboxed_data_path` will continue to be used and the
      // migration attempt will be retried the next time the same network
      // context is created with `trigger_migration` set.
      return SandboxGrantResult::kFailedToGrantSandboxAccessToData;
    }
  }  // SCOPED_UMA_HISTOGRAM_TIMER

  // This covers cases 3. and 4. where a migration has already happened.
  if (migration_already_happened) {
    // Migration succeeded in an earlier attempt and `data_directory` is valid,
    // but clean up any old data that might have failed to delete in the last
    // attempt.
    SandboxGrantResult cleanup_result = CleanUpOldData(params);
    if (cleanup_result != SandboxGrantResult::kSuccess)
      return cleanup_result;
    return SandboxGrantResult::kMigrationAlreadySucceeded;
  }

  SandboxGrantResult result;
  // Reaching here means case 2. where a migration hasn't yet happened, but it's
  // been requested.
  //
  // Now attempt to migrate the data from the `unsandboxed_data_path` to the new
  // `data_directory`. This code can be removed from content once migration has
  // taken place.
  //
  // This code has a three stage process.
  // 1. An attempt is made to copy all the data files from the old location to
  // the new location.
  // 2. A checkpoint file ("NetworkData") is then placed in the new directory to
  // mark that the data there is valid and should be used.
  // 3. The old files are deleted.
  //
  // A failure half way through stage 1 or 2 will mean that the old data should
  // be used instead of the new data. A failure to delete the files will cause
  // a retry attempt next time the same network context is created.
  {
    // Stage 1: Copy the data files. Note: This might copy files over the top of
    // existing files if it was partially successful in an earlier attempt.
    SCOPED_UMA_HISTOGRAM_TIMER("NetworkService.TimeToMigrateData");
    result = MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                           params->file_paths->data_directory.path(),
                           params->file_paths->cookie_database_name,
                           /*is_sql=*/true);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                           params->file_paths->data_directory.path(),
                           params->file_paths->http_server_properties_file_name,
                           /*is_sql=*/false);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(
        *params->file_paths->unsandboxed_data_path,
        params->file_paths->data_directory.path(),
        params->file_paths->transport_security_persister_file_name,
        /*is_sql=*/false);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result =
        MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                      params->file_paths->data_directory.path(),
                      params->file_paths->reporting_and_nel_store_database_name,
                      /*is_sql=*/true);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                           params->file_paths->data_directory.path(),
                           params->file_paths->trust_token_database_name,
                           /*is_sql=*/true);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    // Files all copied successfully. Can now proceed to Stage 2 and write the
    // checkpoint filename.
    base::File checkpoint_file(
        checkpoint_filename,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!checkpoint_file.IsValid())
      return SandboxGrantResult::kFailedToCreateCheckpointFile;
  }  // SCOPED_UMA_HISTOGRAM_TIMER

  // Double check the checkpoint file is there. This should never happen.
  if (!base::PathExists(checkpoint_filename))
    return SandboxGrantResult::kFailedToCreateCheckpointFile;

  // Success, proceed to Stage 3 and clean up old files.
  SandboxGrantResult cleanup_result = CleanUpOldData(params);
  if (cleanup_result != SandboxGrantResult::kSuccess)
    return cleanup_result;

  return SandboxGrantResult::kSuccess;
}

}  // namespace

void GrantSandboxAccessOnThreadPool(
    network::mojom::NetworkContextParamsPtr params,
    base::OnceCallback<void(network::mojom::NetworkContextParamsPtr,
                            SandboxGrantResult)> result_callback) {
  SandboxParameters sandbox_params = {};
#if BUILDFLAG(IS_WIN)
  sandbox_params.lpac_capability_name =
      GetContentClient()->browser()->GetLPACCapabilityNameForNetworkService();
#if DCHECK_IS_ON()
  sandbox_params.sandbox_enabled =
      GetContentClient()->browser()->ShouldSandboxNetworkService();
#endif  // DCHECK_IS_ON()
#endif  // BUILDFLAG(IS_WIN)
  base::OnceCallback<SandboxGrantResult()> worker_task =
      base::BindOnce(&MaybeGrantSandboxAccessToNetworkContextData,
                     sandbox_params, params.get());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      std::move(worker_task),
      base::BindOnce(std::move(result_callback), std::move(params)));
}

}  // namespace content
