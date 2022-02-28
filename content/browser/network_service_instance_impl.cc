// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_instance_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/dcheck_is_on.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/network_service_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/log/net_log_util.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "sql/database.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/windows_version.h"
#include "sandbox/features.h"
#elif BUILDFLAG(IS_ANDROID)
#include "content/common/android/cpu_affinity_setter.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

#if BUILDFLAG(IS_POSIX)
// Environment variable pointing to credential cache file.
constexpr char kKrb5CCEnvName[] = "KRB5CCNAME";
// Environment variable pointing to Kerberos config file.
constexpr char kKrb5ConfEnvName[] = "KRB5_CONFIG";
#endif

bool g_force_create_network_service_directly = false;
mojo::Remote<network::mojom::NetworkService>* g_network_service_remote =
    nullptr;
network::NetworkConnectionTracker* g_network_connection_tracker;
bool g_network_service_is_responding = false;
base::Time g_last_network_service_crash;

// A filename that represents that the data contained within `data_path` has
// been migrated successfully and the data in `unsandboxed_data_path` is now
// invalid.
const base::FilePath::CharType kCheckpointFileName[] =
    FILE_PATH_LITERAL("NetworkDataMigrated");

// A directory name that is created below the http cache path and passed to the
// network context when creating a network context with cache enabled.
// This must be a directory below the main cache path so operations such as
// resetting the cache via HttpCacheParams.reset_cache can function correctly
// as they rely on having access to the parent directory of the cache.
const base::FilePath::CharType kCacheDataDirectoryName[] =
    FILE_PATH_LITERAL("Cache_Data");

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
  // `data_path` during a migration operation.
  kFailedToCopyData = 3,
  // Failed to delete a data file from the `unsandboxed_data_path` after
  // successfully moving it to the `data_path` during a migration operation.
  kFailedToDeleteOldData = 4,
  // Failed to grant the sandbox access to the `http_cache_path`.
  kFailedToGrantSandboxAccessToCache = 5,
  // Failed to grant the sandbox access to the `data_path` so
  // `unsandboxed_data_path` should be used.
  kFailedToGrantSandboxAccessToData = 6,
  // No migration was attempted either because of platform constraints or
  // because the network context had no valid data paths (e.g. in-memory or
  // incognito), or `unsandboxed_data_path` was not specified.
  kDidNotAttemptToGrantSandboxAccess = 7,
  // Failed to create the checkpoint file that indicates that the files in
  // `data_path` are valid.
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

std::unique_ptr<network::NetworkService>& GetLocalNetworkService() {
  static base::SequenceLocalStorageSlot<
      std::unique_ptr<network::NetworkService>>
      service;
  return service.GetOrCreateValue();
}

// If this feature is enabled, the Network Service will run on its own thread
// when running in-process; otherwise it will run on the IO thread.
//
// On Chrome OS, the Network Service must run on the IO thread because
// ProfileIOData and NetworkContext both try to set up NSS, which has to be
// called from the IO thread.
const base::Feature kNetworkServiceDedicatedThread {
  "NetworkServiceDedicatedThread",
#if BUILDFLAG(IS_CHROMEOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

base::Thread& GetNetworkServiceDedicatedThread() {
  static base::NoDestructor<base::Thread> thread{"NetworkService"};
  return *thread;
}

// The instance NetworkService used when hosting the service in-process. This is
// set up by |CreateInProcessNetworkServiceOnThread()| and destroyed by
// |ShutDownNetworkService()|.
network::NetworkService* g_in_process_instance = nullptr;

static NetworkServiceClient* g_client = nullptr;

void CreateInProcessNetworkServiceOnThread(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver) {
#if BUILDFLAG(IS_ANDROID)
  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kBigLittleScheduling,
          features::kBigLittleSchedulingNetworkMainBigParam, false)) {
    SetCpuAffinityForCurrentThread(base::CpuAffinityMode::kBigCoresOnly);
  }
#endif

  // The test interface doesn't need to be implemented in the in-process case.
  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<network::mojom::NetworkServiceTest>) {}));
  g_in_process_instance = new network::NetworkService(
      std::move(registry), std::move(receiver),
      true /* delay_initialization_until_set_client */);
}

// A utility function to make it clear what behavior is expected by the network
// context instance depending on the various errors that can happen during data
// migration.
//
// If this function returns 'true' then the `data_path` should be used (if
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
      // affect the providence of the data contained in `data_path` as the
      // migration could have still occurred.
      //
      // These cases are handled internally and so this case should never be
      // hit. It is undefined behavior to proceed in this case so CHECK here.
      IMMEDIATE_CRASH();
      return false;
    case SandboxGrantResult::kFailedToCreateDataDirectory:
      // A failure to create the `data_path` is fatal, and the
      // `unsandboxed_data_path` should be used.
      return false;
    case SandboxGrantResult::kFailedToCopyData:
      // A failure to copy the data from `unsandboxed_data_path` to the
      // `data_path` is fatal, and the `unsandboxed_data_path` should be used.
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
      // incognito), or `unsandboxed_data_path` was not specified. `data_path`
      // should be used in this case (if present).
      return true;
    case SandboxGrantResult::kFailedToCreateCheckpointFile:
      // This is fatal, as a failure to create the checkpoint file means that
      // the next time the same network context is used, the data in
      // `unsandboxed_data_path` will be re-copied to the new `data_path` and
      // thus any changes to the data will be discarded. So in this case,
      // `unsandboxed_data_path` should be used.
      return false;
    case SandboxGrantResult::kNoMigrationRequested:
      // The caller supplied an `unsandboxed_data_path` but did not trigger a
      // migration so the data should be read from the `unsandboxed_data_path`.
      return false;
    case SandboxGrantResult::kMigrationAlreadySucceeded:
      // Migration has already taken place, so `data_path` contains the valid
      // data.
      return true;
    case SandboxGrantResult::kMigrationAlreadySucceededWithNoAccess:
      // If the sandbox could not be granted access to the new data dir, but the
      // migration has already happened to `data_path`. This means that the
      // sandbox might not have access to the data but `data_path` should still
      // be used because it's been migrated.
      return true;
  }
}

// Takes a cache dir and deletes all files in it except those in 'Cache_Data'
// directory. This can be removed once all caches have been moved to the new
// sub-directory, around M99.
void MaybeDeleteOldCache(const base::FilePath& cache_dir) {
  bool deleted_old_files = false;
  base::FileEnumerator enumerator(
      cache_dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

  for (auto name = enumerator.Next(); !name.empty(); name = enumerator.Next()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    DCHECK_EQ(info.GetName(), name.BaseName());

    if (info.IsDirectory()) {
      if (name.BaseName().value() == kCacheDataDirectoryName)
        continue;
    }
    base::DeletePathRecursively(name);
    deleted_old_files = true;
  }

  base::UmaHistogramBoolean("NetworkService.DeletedOldCacheData",
                            deleted_old_files);
}

void CreateNetworkContextInternal(
    mojo::PendingReceiver<network::mojom::NetworkContext> context,
    network::mojom::NetworkContextParamsPtr params,
    SandboxGrantResult grant_access_result) {
  // These two histograms are logged from elsewhere, so don't log them twice.
  DCHECK(grant_access_result !=
         SandboxGrantResult::kFailedToCreateCacheDirectory);
  DCHECK(grant_access_result !=
         SandboxGrantResult::kFailedToGrantSandboxAccessToCache);
  base::UmaHistogramEnumeration("NetworkService.GrantSandboxResult",
                                grant_access_result);

  if (grant_access_result != SandboxGrantResult::kSuccess &&
      grant_access_result !=
          SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess &&
      grant_access_result != SandboxGrantResult::kNoMigrationRequested &&
      grant_access_result != SandboxGrantResult::kMigrationAlreadySucceeded) {
    PLOG(ERROR) << "Encountered error while migrating network context data or "
                   "granting sandbox access for "
                << (params->file_paths ? params->file_paths->data_path
                                       : base::FilePath())
                << ". Result: " << static_cast<int>(grant_access_result);
  }

  if (!IsSafeToUseDataPath(grant_access_result)) {
    // Unsafe to use new `data_path`. This means that a migration was attempted,
    // and `unsandboxed_data_path` contains the still-valid set of data. Swap
    // the parameters to instruct the network service to use this path for the
    // network context. This of course will mean that if the network service is
    // running sandboxed then this data might not be accessible, but does
    // provide a pathway to user recovery, as the sandbox can just be disabled
    // in this case.
    DCHECK(params->file_paths->unsandboxed_data_path.has_value());
    params->file_paths->data_path = *params->file_paths->unsandboxed_data_path;
  }
  if (params->http_cache_enabled && params->http_cache_path) {
    // Delete any old data except for the "Cache_Data" directory.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(MaybeDeleteOldCache, *params->http_cache_path));
    params->http_cache_path =
        params->http_cache_path->Append(kCacheDataDirectoryName);
  }
  GetNetworkService()->CreateNetworkContext(std::move(context),
                                            std::move(params));
}

// Grants the sandbox access to the specified `path`, which must be a directory
// that exists. Currently this is only implemented on Windows, where the LPAC
// capability name should be supplied in the `sandbox_params` to specify the
// name of the LPAC capability to be applied to the path. Returns true if the
// sandbox was successfully granted access to the path.
bool MaybeGrantAccessToDataPath(const SandboxParameters& sandbox_params,
                                const base::FilePath& path) {
  // There is no need to set file permissions if the network service is running
  // in-process.
  if (IsInProcessNetworkService())
    return true;
  // Only do this on directories.
  if (!base::DirectoryExists(path))
    return false;
#if BUILDFLAG(IS_WIN)
  // On platforms that don't support the LPAC sandbox, do nothing.
  if (!sandbox::features::IsAppContainerSandboxSupported())
    return true;
  DCHECK(!sandbox_params.lpac_capability_name.empty());
  auto ac_sids = base::win::Sid::FromNamedCapabilityVector(
      {sandbox_params.lpac_capability_name.c_str()});
  if (!ac_sids.has_value()) {
    NOTREACHED();
    return false;
  }

  // Grant recursive access to directory. This also means new files in the
  // directory will inherit the ACE.
  return base::win::GrantAccessToPath(
      path, *ac_sids, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | DELETE,
      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE, /*recursive=*/true);
#else
  return true;
#endif  // BUILDFLAG(IS_WIN)
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
                                 const absl::optional<base::FilePath>& filename,
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

// Deletes the old data for a data file called `filename` from `old_path`. If
// `file_path` refers to an SQL database then `is_sql` should be set to true,
// and the journal file will also be deleted.
//
// Returns SandboxGrantResult::kSuccess if the all delete operations completed
// successfully. Returns SandboxGrantResult::kFailedToDeleteData if a file could
// not be deleted.
SandboxGrantResult MaybeDeleteOldData(
    const base::FilePath& old_path,
    const absl::optional<base::FilePath>& filename,
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

// Deletes old data from `unsandboxed_data_path` if a migration operation has
// been successful.
SandboxGrantResult CleanUpOldData(
    network::mojom::NetworkContextParams* params) {
  // Never delete old data unless the checkpoint file exists.
  DCHECK(base::PathExists(
      params->file_paths->data_path.Append(kCheckpointFileName)));

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

scoped_refptr<base::SequencedTaskRunner>& GetNetworkTaskRunnerStorage() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> storage;
  return *storage;
}

void CreateInProcessNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  // If it's specified to run a separate thread for the in-process network
  // service, or if the IO thread isn't initialized because we're in Android's
  // minimal browser mode, then use a dedicated thread.
  if (base::FeatureList::IsEnabled(kNetworkServiceDedicatedThread) ||
      !BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    GetNetworkServiceDedicatedThread().StartWithOptions(std::move(options));
    task_runner = GetNetworkServiceDedicatedThread().task_runner();
  } else {
    task_runner = GetIOThreadTaskRunner({});
  }

  GetNetworkTaskRunnerStorage() = std::move(task_runner);

  GetNetworkTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&CreateInProcessNetworkServiceOnThread,
                                std::move(receiver)));
}

network::mojom::NetworkServiceParamsPtr CreateNetworkServiceParams() {
  network::mojom::NetworkServiceParamsPtr network_service_params =
      network::mojom::NetworkServiceParams::New();
  network_service_params->initial_connection_type =
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType());
  network_service_params->initial_connection_subtype =
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype());
  network_service_params->default_observer =
      g_client->BindURLLoaderNetworkServiceObserver();
  network_service_params->first_party_sets_enabled =
      GetContentClient()->browser()->IsFirstPartySetsEnabled();

#if BUILDFLAG(IS_POSIX)
  // Send Kerberos environment variables to the network service.
  if (IsOutOfProcessNetworkService()) {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string value;
    if (env->HasVar(kKrb5CCEnvName)) {
      env->GetVar(kKrb5CCEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5CCEnvName, value));
    }
    if (env->HasVar(kKrb5ConfEnvName)) {
      env->GetVar(kKrb5ConfEnvName, &value);
      network_service_params->environment.push_back(
          network::mojom::EnvironmentVariable::New(kKrb5ConfEnvName, value));
    }
  }
#endif
  return network_service_params;
}

void CreateNetworkServiceOnIOForTesting(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver,
    base::WaitableEvent* completion_event) {
  if (GetLocalNetworkService()) {
    GetLocalNetworkService()->Bind(std::move(receiver));
    return;
  }

  GetLocalNetworkService() = std::make_unique<network::NetworkService>(
      nullptr /* registry */, std::move(receiver),
      true /* delay_initialization_until_set_client */);
  GetLocalNetworkService()->Initialize(
      network::mojom::NetworkServiceParams::New(),
      true /* mock_network_change_notifier */);
  if (completion_event)
    completion_event->Signal();
}

void BindNetworkChangeManagerReceiver(
    mojo::PendingReceiver<network::mojom::NetworkChangeManager> receiver) {
  GetNetworkService()->GetNetworkChangeManager(std::move(receiver));
}

base::RepeatingClosureList& GetCrashHandlersList() {
  static base::NoDestructor<base::RepeatingClosureList> s_list;
  return *s_list;
}

void OnNetworkServiceCrash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_network_service_remote);
  DCHECK(g_network_service_remote->is_bound());
  DCHECK(!g_network_service_remote->is_connected());
  g_last_network_service_crash = base::Time::Now();
  GetCrashHandlersList().Notify();
}

// Parses the desired granularity of NetLog capturing specified by the command
// line.
net::NetLogCaptureMode GetNetCaptureModeFromCommandLine(
    const base::CommandLine& command_line) {
  base::StringPiece switch_name = network::switches::kNetLogCaptureMode;

  if (command_line.HasSwitch(switch_name)) {
    std::string value = command_line.GetSwitchValueASCII(switch_name);

    if (value == "Default")
      return net::NetLogCaptureMode::kDefault;
    if (value == "IncludeSensitive")
      return net::NetLogCaptureMode::kIncludeSensitive;
    if (value == "Everything")
      return net::NetLogCaptureMode::kEverything;

    // Warn when using the old command line switches.
    if (value == "IncludeCookiesAndCredentials") {
      LOG(ERROR) << "Deprecated value for --" << switch_name
                 << ". Use IncludeSensitive instead";
      return net::NetLogCaptureMode::kIncludeSensitive;
    }
    if (value == "IncludeSocketBytes") {
      LOG(ERROR) << "Deprecated value for --" << switch_name
                 << ". Use Everything instead";
      return net::NetLogCaptureMode::kEverything;
    }

    LOG(ERROR) << "Unrecognized value for --" << switch_name;
  }

  return net::NetLogCaptureMode::kDefault;
}

// Attempts to grant the sandbox access to the file data specified in the
// `params`. This function will also perform a migration of existing data from
// `unsandboxed_data_path` to `data_path` as necessary.
//
// This process has a few stages:
// 1. Create and grant the sandbox access to the cache dir.
// 2. If `data_path` is not specified then the caller is using in-memory storage
// and so there's nothing to do. END.
// 2. If `unsandboxed_data_path` is not specified then the caller is not aware
// of the sandbox or migration, and the steps terminate here with `data_path`
// used by the network context and END.
// 4. If migration has already taken place, regardless of whether it's requested
// this time, grant the sandbox access to the `data_path` (since this needs to
// be done every time), and terminate here with `data_path` being used. END.
// 5. If migration is not requested, then terminate here with
// `unsandboxed_data_path` being used. END.
// 6. At this point, migration has been requested and hasn't already happened,
// so begin a migration attempt. If any of these steps fail, then bail out, and
// `unsandboxed_data_path` is used.
// 7. Grant the sandbox access to the `data_path` (this is done before copying
// the files to use inherited ACLs when copying files on Windows).
// 8. Copy all the data files one by one from the `unsandboxed_data_path` to the
// `data_path`.
// 9. Once all the files have been copied, lay down the Checkpoint file in the
// `data_path`.
// 10. Delete all the original files (if they exist) from
// `unsandboxed_data_path`.
//
// Various failures can occur during this process, and those are represented by
// the SandboxGrantResult. These values are described in more detail above.
SandboxGrantResult MaybeGrantSandboxAccessToNetworkContextData(
    const SandboxParameters& sandbox_params,
    network::mojom::NetworkContextParams* params) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#if BUILDFLAG(IS_WIN)
#if DCHECK_IS_ON()
  params->win_permissions_set = true;
#endif
#endif  // BUILDFLAG(IS_WIN)

  // HTTP cache path is special, and not under `data_path` so must also be
  // granted access. Continue attempting to grant access to the other files if
  // this part fails.
  if (params->http_cache_path && params->http_cache_enabled) {
    SandboxGrantResult cache_result = SandboxGrantResult::kSuccess;
    // The path must exist for the cache ACL to be set. Create if needed.
    if (!base::CreateDirectory(params->http_cache_path.value()))
      cache_result = SandboxGrantResult::kFailedToCreateCacheDirectory;
    if (cache_result == SandboxGrantResult::kSuccess) {
      // Note, this code always grants access to the cache directory even when
      // the sandbox is not enabled. This is a optimization (on Windows) because
      // by setting the ACL on the directory earlier rather than later, it
      // ensures that any new files created by the cache subsystem get the
      // inherited ACE rather than having to set them manually later.
      SCOPED_UMA_HISTOGRAM_TIMER("NetworkService.TimeToGrantCacheAccess");
      if (!MaybeGrantAccessToDataPath(sandbox_params,
                                      params->http_cache_path.value())) {
        PLOG(ERROR) << "Failed to grant sandbox access to cache directory "
                    << *params->http_cache_path;
        cache_result = SandboxGrantResult::kFailedToGrantSandboxAccessToCache;
      }
    }

    // Log a separate histogram entry for failures related to the disk cache
    // here.
    base::UmaHistogramEnumeration("NetworkService.GrantSandboxToCacheResult",
                                  cache_result);
  }

  // No file paths (e.g. in-memory context) so nothing to do.
  if (!params->file_paths)
    return SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess;

  DCHECK(!params->file_paths->data_path.empty());

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
  DCHECK_NE(params->file_paths->data_path,
            *params->file_paths->unsandboxed_data_path);

  // Four cases need to be handled here.
  //
  // 1. No Checkpoint file, and `trigger_migration` is false: Data is still in
  // `unsandboxed_data_path` and sandbox does not need to be granted access. No
  // migration happens.
  // 2. No Checkpoint file, and `trigger_migration` is true: Data is in
  // `unsandboxed_data_path` and needs to be migrated to `data_path`, and the
  // sandbox needs to be granted access to `data_path`.
  // 3. Checkpoint file, and `trigger_migration` is false: Data is in
  // `data_path` (already migrated) and sandbox needs to be granted access to
  // `data_path`.
  // 4. Checkpoint file, and `trigger_migration` is true: Data is in `data_path`
  // (already migrated) and sandbox needs to be granted access to `data_path`.
  // This is the same as above and `trigger_migration` changes nothing, as it's
  // already happened.
  base::FilePath checkpoint_filename =
      params->file_paths->data_path.Append(kCheckpointFileName);
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

  // Create the `data_path` if necessary so access can be granted to it. Note
  // that if a migration has already happened then this does nothing, as the
  // directory already exists.
  if (!base::CreateDirectory(params->file_paths->data_path)) {
    PLOG(ERROR) << "Failed to create network context data directory "
                << params->file_paths->data_path;
    // This is a fatal error, if the `data_path` does not exist then migration
    // cannot be attempted. In this case the network context will operate
    // using `unsandboxed_data_path` and the migration attempt will be retried
    // the next time the same network context is created with
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
                                    params->file_paths->data_path)) {
      PLOG(ERROR)
          << "Failed to grant sandbox access to network context data directory "
          << params->file_paths->data_path;
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
    // Migration succeeded in an earlier attempt and `data_path` is valid, but
    // clean up any old data that might have failed to delete in the last
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
  // `data_path`. This code can be removed from content once migration has taken
  // place.
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
                           params->file_paths->data_path,
                           params->file_paths->cookie_database_name,
                           /*is_sql=*/true);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                           params->file_paths->data_path,
                           params->file_paths->http_server_properties_file_name,
                           /*is_sql=*/false);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(
        *params->file_paths->unsandboxed_data_path,
        params->file_paths->data_path,
        params->file_paths->transport_security_persister_file_name,
        /*is_sql=*/false);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result =
        MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                      params->file_paths->data_path,
                      params->file_paths->reporting_and_nel_store_database_name,
                      /*is_sql=*/true);
    if (result != SandboxGrantResult::kSuccess)
      return result;

    result = MaybeCopyData(*params->file_paths->unsandboxed_data_path,
                           params->file_paths->data_path,
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

class NetworkServiceInstancePrivate {
 public:
  // Opens the specified file, blocking until the file is open. Used to open
  // files specified by network::switches::kLogNetLog or
  // network::switches::kSSLKeyLogFile. Since these arguments can be used to
  // debug startup behavior, asynchronously opening the file on another thread
  // would result in losing data, hence the need for blocking open operations.
  // |file_flags| specifies the flags passed to the base::File constructor call.
  //
  // ThreadRestrictions needs to be able to friend the class/method to allow
  // blocking, but can't friend CONTENT_EXPORT methods, so have it friend
  // NetworkServiceInstancePrivate instead of GetNetworkService().
  static base::File BlockingOpenFile(const base::FilePath& path,
                                     int file_flags) {
    base::ScopedAllowBlocking allow_blocking;
    return base::File(path, file_flags);
  }
};

network::mojom::NetworkService* GetNetworkService() {
  if (!g_network_service_remote)
    g_network_service_remote = new mojo::Remote<network::mojom::NetworkService>;
  if (!g_network_service_remote->is_bound() ||
      !g_network_service_remote->is_connected()) {
    bool service_was_bound = g_network_service_remote->is_bound();
    g_network_service_remote->reset();
    if (GetContentClient()->browser()->IsShuttingDown()) {
      // This happens at system shutdown, since in other scenarios the network
      // process would only be torn down once the message loop stopped running.
      // We don't want to start the network service again so just create message
      // pipe that's not bound to stop consumers from requesting creation of the
      // service.
      auto receiver = g_network_service_remote->BindNewPipeAndPassReceiver();
      auto leaked_pipe = receiver.PassPipe().release();
    } else {
      if (!g_force_create_network_service_directly) {
        mojo::PendingReceiver<network::mojom::NetworkService> receiver =
            g_network_service_remote->BindNewPipeAndPassReceiver();
        g_network_service_remote->set_disconnect_handler(
            base::BindOnce(&OnNetworkServiceCrash));
        if (IsInProcessNetworkService()) {
          CreateInProcessNetworkService(std::move(receiver));
        } else {
          if (service_was_bound)
            LOG(ERROR) << "Network service crashed, restarting service.";
          ServiceProcessHost::Launch(std::move(receiver),
                                     ServiceProcessHost::Options()
                                         .WithDisplayName(u"Network Service")
                                         .Pass());
        }
      } else {
        // This should only be reached in unit tests.
        if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
          CreateNetworkServiceOnIOForTesting(
              g_network_service_remote->BindNewPipeAndPassReceiver(),
              /*completion_event=*/nullptr);
        } else {
          base::WaitableEvent event;
          GetIOThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(
                  CreateNetworkServiceOnIOForTesting,
                  g_network_service_remote->BindNewPipeAndPassReceiver(),
                  base::Unretained(&event)));
          event.Wait();
        }
      }

      delete g_client;  // In case we're recreating the network service.
      g_client = new NetworkServiceClient();

      // Call SetClient before creating NetworkServiceClient, as the latter
      // might make requests to NetworkService that depend on initialization.
      (*g_network_service_remote)->SetParams(CreateNetworkServiceParams());
      g_client->OnNetworkServiceInitialized(g_network_service_remote->get());

      g_network_service_is_responding = false;
      g_network_service_remote->QueryVersion(base::BindOnce(
          [](base::Time start_time, uint32_t) {
            g_network_service_is_responding = true;
            base::TimeDelta delta = base::Time::Now() - start_time;
            UMA_HISTOGRAM_MEDIUM_TIMES("NetworkService.TimeToFirstResponse",
                                       delta);
            if (g_last_network_service_crash.is_null()) {
              UMA_HISTOGRAM_MEDIUM_TIMES(
                  "NetworkService.TimeToFirstResponse.OnStartup", delta);
            } else {
              UMA_HISTOGRAM_MEDIUM_TIMES(
                  "NetworkService.TimeToFirstResponse.AfterCrash", delta);
            }
          },
          base::Time::Now()));

      const base::CommandLine* command_line =
          base::CommandLine::ForCurrentProcess();
      if (command_line->HasSwitch(network::switches::kLogNetLog)) {
        base::FilePath log_path =
            command_line->GetSwitchValuePath(network::switches::kLogNetLog);
        if (log_path.empty()) {
          log_path = GetContentClient()->browser()->GetNetLogDefaultDirectory();
          if (!log_path.empty())
            log_path = log_path.Append(FILE_PATH_LITERAL("netlog.json"));
        }

        base::File file = NetworkServiceInstancePrivate::BlockingOpenFile(
            log_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
        if (!file.IsValid()) {
          LOG(ERROR) << "Failed opening NetLog: " << log_path.value();
        } else {
          (*g_network_service_remote)
              ->StartNetLog(
                  std::move(file),
                  GetNetCaptureModeFromCommandLine(*command_line),
                  GetContentClient()->browser()->GetNetLogConstants());
        }
      }

      base::FilePath ssl_key_log_path;
      if (command_line->HasSwitch(network::switches::kSSLKeyLogFile)) {
        UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                  SSLKeyLogFileAction::kSwitchFound);
        ssl_key_log_path =
            command_line->GetSwitchValuePath(network::switches::kSSLKeyLogFile);
        LOG_IF(WARNING, ssl_key_log_path.empty())
            << "ssl-key-log-file argument missing";
      } else {
        std::unique_ptr<base::Environment> env(base::Environment::Create());
        std::string env_str;
        if (env->GetVar("SSLKEYLOGFILE", &env_str)) {
          UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                    SSLKeyLogFileAction::kEnvVarFound);
#if BUILDFLAG(IS_WIN)
          // base::Environment returns environment variables in UTF-8 on
          // Windows.
          ssl_key_log_path = base::FilePath(base::UTF8ToWide(env_str));
#else
          ssl_key_log_path = base::FilePath(env_str);
#endif
        }
      }

      if (!ssl_key_log_path.empty()) {
        base::File file = NetworkServiceInstancePrivate::BlockingOpenFile(
            ssl_key_log_path,
            base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
        if (!file.IsValid()) {
          LOG(ERROR) << "Failed opening SSL key log file: "
                     << ssl_key_log_path.value();
        } else {
          UMA_HISTOGRAM_ENUMERATION(kSSLKeyLogFileHistogram,
                                    SSLKeyLogFileAction::kLogFileEnabled);
          (*g_network_service_remote)->SetSSLKeyLogFile(std::move(file));
        }
      }

      GetContentClient()->browser()->OnNetworkServiceCreated(
          g_network_service_remote->get());
    }
  }
  return g_network_service_remote->get();
}

base::CallbackListSubscription RegisterNetworkServiceCrashHandler(
    base::RepeatingClosure handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!handler.is_null());

  return GetCrashHandlersList().Add(std::move(handler));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
net::NetworkChangeNotifier* GetNetworkChangeNotifier() {
  return BrowserMainLoop::GetInstance()->network_change_notifier();
}
#endif

void FlushNetworkServiceInstanceForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (g_network_service_remote)
    g_network_service_remote->FlushForTesting();
}

network::NetworkConnectionTracker* GetNetworkConnectionTracker() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!g_network_connection_tracker) {
    g_network_connection_tracker = new network::NetworkConnectionTracker(
        base::BindRepeating(&BindNetworkChangeManagerReceiver));
  }
  return g_network_connection_tracker;
}

void GetNetworkConnectionTrackerFromUIThread(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTaskAndReplyWithResult(FROM_HERE,
                                   base::BindOnce(&GetNetworkConnectionTracker),
                                   std::move(callback));
}

network::NetworkConnectionTrackerAsyncGetter
CreateNetworkConnectionTrackerAsyncGetter() {
  return base::BindRepeating(&content::GetNetworkConnectionTrackerFromUIThread);
}

void SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (g_network_connection_tracker != network_connection_tracker) {
    DCHECK(!g_network_connection_tracker || !network_connection_tracker);
    g_network_connection_tracker = network_connection_tracker;
  }
}

const scoped_refptr<base::SequencedTaskRunner>& GetNetworkTaskRunner() {
  DCHECK(IsInProcessNetworkService());
  return GetNetworkTaskRunnerStorage();
}

void ForceCreateNetworkServiceDirectlyForTesting() {
  g_force_create_network_service_directly = true;
}

void ResetNetworkServiceForTesting() {
  ShutDownNetworkService();
}

void ShutDownNetworkService() {
  delete g_network_service_remote;
  g_network_service_remote = nullptr;
  delete g_client;
  g_client = nullptr;
  if (g_in_process_instance) {
    GetNetworkTaskRunner()->DeleteSoon(FROM_HERE, g_in_process_instance);
    g_in_process_instance = nullptr;
  }
  GetNetworkTaskRunnerStorage().reset();
}

NetworkServiceAvailability GetNetworkServiceAvailability() {
  if (!g_network_service_remote)
    return NetworkServiceAvailability::NOT_CREATED;
  else if (!g_network_service_remote->is_bound())
    return NetworkServiceAvailability::NOT_BOUND;
  else if (!g_network_service_remote->is_connected())
    return NetworkServiceAvailability::ENCOUNTERED_ERROR;
  else if (!g_network_service_is_responding)
    return NetworkServiceAvailability::NOT_RESPONDING;
  else
    return NetworkServiceAvailability::AVAILABLE;
}

base::TimeDelta GetTimeSinceLastNetworkServiceCrash() {
  if (g_last_network_service_crash.is_null())
    return base::TimeDelta();
  return base::Time::Now() - g_last_network_service_crash;
}

void PingNetworkService(base::OnceClosure closure) {
  GetNetworkService();
  // Unfortunately, QueryVersion requires a RepeatingCallback.
  g_network_service_remote->QueryVersion(base::BindOnce(
      [](base::OnceClosure closure, uint32_t) {
        if (closure)
          std::move(closure).Run();
      },
      std::move(closure)));
}

namespace {

cert_verifier::mojom::CertVerifierServiceFactory*
    g_cert_verifier_service_factory_for_testing = nullptr;

mojo::PendingRemote<cert_verifier::mojom::CertVerifierService>
GetNewCertVerifierServiceRemote(
    cert_verifier::mojom::CertVerifierServiceFactory*
        cert_verifier_service_factory,
    cert_verifier::mojom::CertVerifierCreationParamsPtr creation_params) {
  mojo::PendingRemote<cert_verifier::mojom::CertVerifierService>
      cert_verifier_remote;
  cert_verifier_service_factory->GetNewCertVerifier(
      cert_verifier_remote.InitWithNewPipeAndPassReceiver(),
      std::move(creation_params));
  return cert_verifier_remote;
}

void RunInProcessCertVerifierServiceFactory(
    mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceFactory>
        receiver) {
#if BUILDFLAG(IS_CHROMEOS)
  // See the comment in GetCertVerifierServiceFactory() for the thread-affinity
  // of the CertVerifierService.
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
#else
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
#endif
  static base::SequenceLocalStorageSlot<
      std::unique_ptr<cert_verifier::CertVerifierServiceFactoryImpl>>
      service_factory_slot;
  service_factory_slot.GetOrCreateValue() =
      std::make_unique<cert_verifier::CertVerifierServiceFactoryImpl>(
          std::move(receiver));
}

// Owns the CertVerifierServiceFactory used by the browser.
// Lives on the UI thread.
mojo::Remote<cert_verifier::mojom::CertVerifierServiceFactory>&
GetCertVerifierServiceFactoryRemoteStorage() {
  static base::SequenceLocalStorageSlot<
      mojo::Remote<cert_verifier::mojom::CertVerifierServiceFactory>>
      cert_verifier_service_factory_remote;
  return cert_verifier_service_factory_remote.GetOrCreateValue();
}

// Returns a pointer to a CertVerifierServiceFactory usable on the UI thread.
cert_verifier::mojom::CertVerifierServiceFactory*
GetCertVerifierServiceFactory() {
  if (g_cert_verifier_service_factory_for_testing)
    return g_cert_verifier_service_factory_for_testing;

  mojo::Remote<cert_verifier::mojom::CertVerifierServiceFactory>&
      factory_remote_storage = GetCertVerifierServiceFactoryRemoteStorage();
  if (!factory_remote_storage.is_bound() ||
      !factory_remote_storage.is_connected()) {
    factory_remote_storage.reset();
#if BUILDFLAG(IS_CHROMEOS)
    // In-process CertVerifierService in Ash and Lacros should run on the IO
    // thread because it interacts with IO-bound NSS and ChromeOS user slots.
    // See for example InitializeNSSForChromeOSUser() or
    // CertDbInitializerIOImpl.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&RunInProcessCertVerifierServiceFactory,
                       factory_remote_storage.BindNewPipeAndPassReceiver()));
#else
    RunInProcessCertVerifierServiceFactory(
        factory_remote_storage.BindNewPipeAndPassReceiver());
#endif
  }
  return factory_remote_storage.get();
}

}  // namespace

network::mojom::CertVerifierServiceRemoteParamsPtr GetCertVerifierParams(
    cert_verifier::mojom::CertVerifierCreationParamsPtr
        cert_verifier_creation_params) {
  return network::mojom::CertVerifierServiceRemoteParams::New(
      GetNewCertVerifierServiceRemote(
          GetCertVerifierServiceFactory(),
          std::move(cert_verifier_creation_params)));
}

void SetCertVerifierServiceFactoryForTesting(
    cert_verifier::mojom::CertVerifierServiceFactory* service_factory) {
  g_cert_verifier_service_factory_for_testing = service_factory;
}

void CreateNetworkContextInNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkContext> context,
    network::mojom::NetworkContextParamsPtr params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SandboxParameters sandbox_params = {};
#if BUILDFLAG(IS_ANDROID)
  // On Android, if a cookie_manager pending receiver was passed then migration
  // should not be attempted as the cookie file is already being accessed by the
  // browser instance.
  if (params->cookie_manager) {
    if (params->file_paths) {
      // No migration should ever be attempted under this configuration.
      DCHECK(!params->file_paths->unsandboxed_data_path);
    }
    CreateNetworkContextInternal(
        std::move(context), std::move(params),
        SandboxGrantResult::kDidNotAttemptToGrantSandboxAccess);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
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
      base::BindOnce(&CreateNetworkContextInternal, std::move(context),
                     std::move(params)));
}

}  // namespace content
