// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/constants.h"
#include "content/browser/media/media_license_database.h"
#include "content/browser/media/media_license_storage_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_features.h"
#include "media/cdm/cdm_type.h"
#include "media/media_buildflags.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "sql/database.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/widevine/cdm/buildflags.h"  // nogncheck
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
#include "content/public/common/cdm_info.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

namespace content {

namespace {

// Creates a task runner suitable for running SQLite database operations.
scoped_refptr<base::SequencedTaskRunner> CreateDatabaseTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to a
  // storage key's directory operations.
  return base::ThreadPool::CreateSequencedTaskRunner({
      // Needed for file I/O.
      base::MayBlock(),

      // Reasonable compromise, given that a few database operations are
      // blocking, while most operations are not. We should be able to do better
      // when we get scheduling APIs on the Web Platform.
      base::TaskPriority::USER_VISIBLE,

      // Needed to allow for clearing site data on shutdown.
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
  });
}

// TODO(crbug.com/1231162): Yes, this code is ugly. It is only in place while we
// migrate to the new media license backend.
absl::optional<media::CdmType> GetCdmTypeFromFileSystemId(
    const std::string& file_system_id) {
  if (file_system_id == "application_x-ppapi-clearkey-cdm") {
    // `kClearKeyCdmType` from media/cdm/cdm_paths.h
    return media::CdmType{
        base::Token{0x3a2e0fadde4bd1b7ull, 0xcb90df3e240d1694ull},
        "application_x-ppapi-clearkey-cdm"};
  }
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  else if (file_system_id == "application_chromeos-cdm-factory-daemon") {
    return kChromeOsCdmType;
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

#if BUILDFLAG(ENABLE_WIDEVINE)
  else if (file_system_id == "application_x-ppapi-widevine-cdm") {
    return kWidevineCdmType;
  }
#if BUILDFLAG(IS_WIN)
  else if (file_system_id == "") {
    return kMediaFoundationWidevineCdmType;
  }
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)
  else if (file_system_id == "test_file_system") {
    // Used in migration tests in cdm_storage_impl_unittest.cc
    return media::CdmType{base::Token{1234, 5678}, "test_file_system"};
  } else if (file_system_id == "different_plugin") {
    // Used in migration tests in cdm_storage_impl_unittest.cc
    return media::CdmType{base::Token{8765, 4321}, "different_plugin"};
  }

  // `file_system_id` doesn't match a known CDM type.
  return absl::nullopt;
}

base::flat_map<blink::StorageKey, std::vector<MediaLicenseManager::CdmFileId>>
GetMediaLicensesOnFileTaskRunner(
    scoped_refptr<storage::FileSystemContext> context) {
  DCHECK(context);

  storage::PluginPrivateFileSystemBackend* plugin_private_backend =
      context->plugin_private_backend();

  auto storage_keys =
      plugin_private_backend->GetStorageKeysForTypeOnFileTaskRunner(
          storage::kFileSystemTypePluginPrivate);

  if (storage_keys.empty())
    return {};

  return base::MakeFlatMap<blink::StorageKey,
                           std::vector<MediaLicenseManager::CdmFileId>>(
      storage_keys, /*comp=*/{},
      [&plugin_private_backend, &context](const auto& storage_key) {
        std::vector<MediaLicenseManager::CdmFileId> cdm_files_for_storage_key;
        auto cdm_files = plugin_private_backend
                             ->GetMediaLicenseFilesForOriginOnFileTaskRunner(
                                 context.get(), storage_key.origin());
        for (const auto& cdm_file : cdm_files) {
          auto maybe_cdm_type =
              GetCdmTypeFromFileSystemId(cdm_file.legacy_file_system_id);
          if (!maybe_cdm_type.has_value())
            continue;

          cdm_files_for_storage_key.emplace_back(cdm_file.name,
                                                 maybe_cdm_type.value());
        }

        return std::make_pair(storage_key,
                              std::move(cdm_files_for_storage_key));
      });
}

void DidReadFiles(
    scoped_refptr<storage::FileSystemContext> context,
    base::OnceCallback<
        void(std::vector<MediaLicenseManager::CdmFileIdAndContents>)> callback,
    const std::vector<MediaLicenseManager::CdmFileIdAndContents>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't bother migrating empty files.
  std::vector<MediaLicenseManager::CdmFileIdAndContents> files_to_migrate;
  base::ranges::for_each(
      files,
      [&files_to_migrate](MediaLicenseManager::CdmFileIdAndContents file) {
        if (!file.data.empty())
          files_to_migrate.emplace_back(std::move(file));
      });

  std::move(callback).Run(std::move(files_to_migrate));
}

void DidReadFile(
    std::unique_ptr<storage::FileStreamReader> /*reader*/,
    scoped_refptr<net::IOBufferWithSize> buffer,
    MediaLicenseManager::CdmFileId file,
    base::OnceCallback<void(MediaLicenseManager::CdmFileIdAndContents)>
        callback,
    int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != buffer->size()) {
    std::move(callback).Run({file, {}});
    return;
  }

  std::vector<uint8_t> data(buffer->data(), buffer->data() + buffer->size());
  std::move(callback).Run({file, std::move(data)});
}

void DidGetLength(
    std::unique_ptr<storage::FileStreamReader> reader,
    MediaLicenseManager::CdmFileId file,
    base::OnceCallback<void(MediaLicenseManager::CdmFileIdAndContents)>
        callback,
    int64_t result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // No need to Read() from `reader` if the file length is 0.
  if (result <= 0) {
    std::move(callback).Run({file, {}});
    return;
  }

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(result);
  auto* reader_ptr = reader.get();
  reader_ptr->Read(buffer.get(), buffer->size(),
                   base::BindOnce(&DidReadFile, std::move(reader), buffer,
                                  std::move(file), std::move(callback)));
}

void ReadFiles(
    scoped_refptr<storage::FileSystemContext> context,
    std::string file_system_root_uri,
    std::vector<MediaLicenseManager::CdmFileId> files,
    base::OnceCallback<void(
        std::vector<MediaLicenseManager::CdmFileIdAndContents>)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!files.empty());

  // Kick off a number of read file operations and collect the results.
  // Bind `context` to keep it alive while reading files.
  auto* context_ptr = context.get();
  auto barrier =
      base::BarrierCallback<MediaLicenseManager::CdmFileIdAndContents>(
          files.size(), base::BindOnce(&DidReadFiles, std::move(context),
                                       std::move(callback)));

  for (const auto& file : files) {
    // Adapted from CdmFileImpl::CreateFileSystemURL().
    const GURL crack_url = GURL(file_system_root_uri + file.name);
    const blink::StorageKey crack_storage_key =
        blink::StorageKey(url::Origin::Create(crack_url));
    auto url = context_ptr->CrackURL(crack_url, crack_storage_key);
    auto reader = context_ptr->CreateFileStreamReader(
        url, 0, storage::kMaximumLength, base::Time());
    if (!reader) {
      barrier.Run({file, {}});
      continue;
    }
    auto* reader_ptr = reader.get();
    auto result = reader_ptr->GetLength(base::BindOnce(
        &DidGetLength, std::move(reader), std::move(file), barrier));
    // The GetLength() call is expected to run asynchronously.
    DCHECK_EQ(result, net::ERR_IO_PENDING);
  }
}

void WriteFilesOnDbThread(
    std::vector<MediaLicenseManager::CdmFileIdAndContents> files,
    const base::FilePath& database_path) {
  auto db = std::make_unique<MediaLicenseDatabase>(database_path);

  for (auto& file : files) {
    db->OpenFile(file.file.cdm_type, file.file.name);
    db->WriteFile(file.file.cdm_type, file.file.name, file.data);
  }
}

}  // namespace

MediaLicenseManager::CdmFileId::CdmFileId(const std::string& name,
                                          const media::CdmType& cdm_type)
    : name(name), cdm_type(cdm_type) {}
MediaLicenseManager::CdmFileId::CdmFileId(const CdmFileId&) = default;
MediaLicenseManager::CdmFileId::~CdmFileId() = default;

MediaLicenseManager::CdmFileIdAndContents::CdmFileIdAndContents(
    const CdmFileId& file,
    std::vector<uint8_t> data)
    : file(file), data(std::move(data)) {}
MediaLicenseManager::CdmFileIdAndContents::CdmFileIdAndContents(
    const CdmFileIdAndContents&) = default;
MediaLicenseManager::CdmFileIdAndContents::~CdmFileIdAndContents() = default;

MediaLicenseManager::MediaLicenseManager(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool in_memory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : file_system_context_(std::move(file_system_context)),
      db_runner_(CreateDatabaseTaskRunner()),
      in_memory_(in_memory),
      special_storage_policy_(std::move(special_storage_policy)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      // Using a raw pointer is safe since `quota_client_` is owned by
      // this instance.
      quota_client_(this),
      quota_client_receiver_(&quota_client_) {
  if (quota_manager_proxy_) {
    // Quota client assumes all backends have registered.
    quota_manager_proxy_->RegisterClient(
        quota_client_receiver_.BindNewPipeAndPassRemote(),
        storage::QuotaClientType::kMediaLicense,
        {blink::mojom::StorageType::kTemporary});
  }

  if (base::FeatureList::IsEnabled(features::kMediaLicenseBackend)) {
    // Ensure the file system context is kept alive until we're done migrating
    // media license data from the Plugin Private File System to this backend.
    MigrateMediaLicenses(
        base::BindOnce([](scoped_refptr<storage::FileSystemContext>) {},
                       base::WrapRefCounted(context().get())));
  }
}

MediaLicenseManager::~MediaLicenseManager() = default;

void MediaLicenseManager::MigrateMediaLicenses(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(plugin_private_data_migration_closure_.is_null());
  plugin_private_data_migration_closure_ = std::move(done_closure);

  context()->default_file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetMediaLicensesOnFileTaskRunner, context()),
      base::BindOnce(&MediaLicenseManager::DidGetMediaLicenses,
                     weak_factory_.GetWeakPtr()));
}

void MediaLicenseManager::DidGetMediaLicenses(
    base::flat_map<blink::StorageKey, std::vector<CdmFileId>> files_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  if (files_map.empty()) {
    DidMigrateMediaLicenses();
    return;
  }

  // Kick off migration process for each storage key.
  base::RepeatingClosure barrier = base::BarrierClosure(
      files_map.size(),
      base::BindPostTask(
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(&MediaLicenseManager::DidMigrateMediaLicenses,
                         weak_factory_.GetWeakPtr())));

  for (auto& storage_key_and_files : files_map) {
    std::vector<CdmFileId> files = std::move(storage_key_and_files.second);
    if (files.empty()) {
      barrier.Run();
      continue;
    }
    const blink::StorageKey& storage_key = storage_key_and_files.first;
    quota_manager_proxy()->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&MediaLicenseManager::OpenPluginFileSystemsForStorageKey,
                       weak_factory_.GetWeakPtr(), storage_key,
                       std::move(files), barrier));
  }
}

void MediaLicenseManager::OpenPluginFileSystemsForStorageKey(
    const blink::StorageKey& storage_key,
    std::vector<CdmFileId> files,
    base::OnceClosure done_migrating_storage_key_closure,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  if (!result.ok()) {
    std::move(done_migrating_storage_key_closure).Run();
    return;
  }

  // Organize files by plugin name.
  base::flat_map<std::string, std::vector<CdmFileId>> files_by_plugin_name;
  for (auto& file : files) {
    files_by_plugin_name[file.cdm_type.legacy_file_system_id].emplace_back(
        std::move(file));
  }

  auto barrier = base::BarrierCallback<std::vector<CdmFileIdAndContents>>(
      files_by_plugin_name.size(),
      base::BindOnce(&MediaLicenseManager::DidReadFilesForStorageKey,
                     weak_factory_.GetWeakPtr(), storage_key,
                     result->ToBucketLocator(),
                     std::move(done_migrating_storage_key_closure)));

  for (const auto& [plugin_name, files] : files_by_plugin_name) {
    // Register and open a file system for this plugin type.
    std::string fsid =
        storage::IsolatedContext::GetInstance()
            ->RegisterFileSystemForVirtualPath(
                storage::kFileSystemTypePluginPrivate,
                storage::kPluginPrivateRootName, base::FilePath());
    DCHECK(storage::ValidateIsolatedFileSystemId(fsid));

    std::string file_system_root_uri =
        storage::GetIsolatedFileSystemRootURIString(
            storage_key.origin().GetURL(), fsid,
            storage::kPluginPrivateRootName);

    context()->OpenPluginPrivateFileSystem(
        storage_key.origin(),
        storage::FileSystemType::kFileSystemTypePluginPrivate, fsid,
        plugin_name,
        storage::OpenFileSystemMode::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
        base::BindOnce(&MediaLicenseManager::DidOpenPluginFileSystem,
                       weak_factory_.GetWeakPtr(), storage_key,
                       std::move(files), std::move(file_system_root_uri),
                       barrier));
  }
}

void MediaLicenseManager::DidOpenPluginFileSystem(
    const blink::StorageKey& storage_key,
    std::vector<CdmFileId> files,
    std::string file_system_root_uri,
    base::OnceCallback<void(std::vector<CdmFileIdAndContents>)> callback,
    base::File::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  if (error != base::File::FILE_OK) {
    std::move(callback).Run({});
    return;
  }

  auto wrapped_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadFiles, context(), std::move(file_system_root_uri),
                     std::move(files), std::move(wrapped_callback)));
}

void MediaLicenseManager::DidReadFilesForStorageKey(
    const blink::StorageKey& storage_key,
    const storage::BucketLocator& bucket_locator,
    base::OnceClosure done_migrating_storage_key_closure,
    std::vector<std::vector<CdmFileIdAndContents>> collected_files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  // Flatten collected files into one vector.
  std::vector<CdmFileIdAndContents> files;
  for (auto& file_list : collected_files) {
    for (auto& file : file_list) {
      // Empty files should have been stripped out.
      DCHECK(!file.data.empty());
      files.emplace_back(std::move(file));
    }
  }

  if (files.empty()) {
    std::move(done_migrating_storage_key_closure).Run();
    return;
  }

  db_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WriteFilesOnDbThread, std::move(files),
                     GetDatabasePath(bucket_locator)),
      std::move(done_migrating_storage_key_closure));
}

void MediaLicenseManager::DidMigrateMediaLicenses() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  // Delete %profile/File System/Plugins since media license data is the only
  // thing stored in the Plugin Private File System.
  auto plugin_path = context()->plugin_private_backend()->base_path();

  context()->default_file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     plugin_path),
      base::BindOnce(&MediaLicenseManager::DidClearPluginPrivateData,
                     weak_factory_.GetWeakPtr()));
}

void MediaLicenseManager::DidClearPluginPrivateData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!plugin_private_data_migration_closure_.is_null());

  std::move(plugin_private_data_migration_closure_).Run();

  // Now that data has been migrated, kick off binding the pending receivers.
  for (const auto& receivers : pending_receivers_) {
    const auto& storage_key = receivers.first;
    // Get the default bucket for `storage_key`.
    quota_manager_proxy()->UpdateOrCreateBucket(
        storage::BucketInitParams::ForDefaultBucket(storage_key),
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&MediaLicenseManager::DidGetBucket,
                       weak_factory_.GetWeakPtr(), storage_key));
  }
}

void MediaLicenseManager::OpenCdmStorage(
    const BindingContext& binding_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& storage_key = binding_context.storage_key;
  auto it_hosts = hosts_.find(storage_key);
  if (it_hosts != hosts_.end()) {
    // A storage host for this storage key already exists.
    it_hosts->second->BindReceiver(binding_context, std::move(receiver));
    return;
  }

  auto& receiver_list = pending_receivers_[storage_key];
  receiver_list.emplace_back(binding_context, std::move(receiver));
  if (receiver_list.size() > 1 ||
      !plugin_private_data_migration_closure_.is_null()) {
    // If a pending receiver for this storage key already existed, there is
    // an in-flight `UpdateOrCreateBucket()` call for this storage key. If we're
    // in the process of migrating data from the plugin private file system,
    // pending receivers will be handled in `DidClearPluginPrivateData()`.
    return;
  }

  // Get the default bucket for `storage_key`.
  quota_manager_proxy()->UpdateOrCreateBucket(
      storage::BucketInitParams::ForDefaultBucket(storage_key),
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&MediaLicenseManager::DidGetBucket,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void MediaLicenseManager::DidGetBucket(
    const blink::StorageKey& storage_key,
    storage::QuotaErrorOr<storage::BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = pending_receivers_.find(storage_key);
  if (it == pending_receivers_.end()) {
    // No receivers to bind.
    // TODO(crbug.com/1231162): This case can only be hit
    // when the migration code is kicked off after `OpenCdmStorage()` has
    // already been called, since `OpenCdmStorage()` will not call
    // `UpdateOrCreateBucket()` while there is an in-progress migration. Change
    // this to a DCHECK once the migration logic is removed.
    return;
  }

  auto receivers_list = std::move(it->second);
  pending_receivers_.erase(it);
  DCHECK_GT(receivers_list.size(), 0u);

  storage::BucketLocator bucket_locator;
  if (result.ok()) {
    bucket_locator = result->ToBucketLocator();
  } else {
    // Use the null locator, but update the `storage_key` field so
    // `storage_host` can be identified when it is to be removed from `hosts_`.
    // We could consider falling back to using an in-memory database in this
    // case, but failing here seems easier to reason about from a website
    // author's point of view.
    DCHECK(bucket_locator.id.is_null());
    bucket_locator.storage_key = storage_key;
  }

  // All receivers associated with `storage_key` will be bound to the same host.
  auto storage_host =
      std::make_unique<MediaLicenseStorageHost>(this, bucket_locator);

  for (auto& context_and_receiver : receivers_list) {
    storage_host->BindReceiver(context_and_receiver.first,
                               std::move(context_and_receiver.second));
  }

  hosts_.emplace(storage_key, std::move(storage_host));
}

void MediaLicenseManager::DeleteBucketData(
    const storage::BucketLocator& bucket,
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it_hosts = hosts_.find(bucket.storage_key);
  if (it_hosts != hosts_.end()) {
    // Let the host gracefully handle data deletion.
    it_hosts->second->DeleteBucketData(
        base::BindOnce(&MediaLicenseManager::DidDeleteBucketData,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // If we have an in-memory profile, any data for the storage key would have
  // lived in the associated MediaLicenseStorageHost.
  if (in_memory()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  // Otherwise delete database file.
  auto path = GetDatabasePath(bucket);
  db_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&sql::Database::Delete, path),
      base::BindOnce(&MediaLicenseManager::DidDeleteBucketData,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaLicenseManager::DidDeleteBucketData(
    storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run(success ? blink::mojom::QuotaStatusCode::kOk
                                  : blink::mojom::QuotaStatusCode::kUnknown);
}

base::FilePath MediaLicenseManager::GetDatabasePath(
    const storage::BucketLocator& bucket_locator) {
  if (in_memory())
    return base::FilePath();

  auto media_license_dir = quota_manager_proxy()->GetClientBucketPath(
      bucket_locator, storage::QuotaClientType::kMediaLicense);
  return media_license_dir.Append(storage::kMediaLicenseDatabaseFileName);
}

void MediaLicenseManager::OnHostReceiverDisconnect(
    MediaLicenseStorageHost* host,
    base::PassKey<MediaLicenseStorageHost> pass_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host);

  if (in_memory()) {
    // Don't delete `host` for an in-memory profile, since the data is not safe
    // to delete yet. For example, a site may be re-visited within the same
    // incognito session. `host` will be destroyed when `this` is destroyed.
    return;
  }

  DCHECK_GT(hosts_.count(host->storage_key()), 0ul);
  DCHECK_EQ(hosts_[host->storage_key()].get(), host);

  if (!host->has_empty_receiver_set())
    return;

  size_t count_removed = hosts_.erase(host->storage_key());
  DCHECK_EQ(count_removed, 1u);
}

}  // namespace content
