// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/fuchsia_cdm_manager.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fpromise/promise.h>

#include <optional>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/mojo/services/fuchsia_cdm_provisioning_fetcher_impl.h"
#include "url/origin.h"

namespace media {

namespace {

struct CdmDirectoryInfo {
  base::FilePath path;
  base::Time last_used;
  uint64_t size_bytes;
};

// Enumerates all the files in the directory to determine its size and
// the most recent "last used" time.
// The implementation is based on base::ComputeDirectorySize(), with the
// addition of most-recently-modified calculation, and inclusion of directory
// node sizes toward the total.
CdmDirectoryInfo GetCdmDirectoryInfo(const base::FilePath& path) {
  uint64_t directory_size = 0;
  base::Time last_used;
  base::FileEnumerator enumerator(
      path, true /* recursive */,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  while (!enumerator.Next().empty()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    if (info.GetSize() > 0) {
      directory_size += info.GetSize();
    }
    last_used = std::max(last_used, info.GetLastModifiedTime());
  }
  return {
      .path = path,
      .last_used = last_used,
      .size_bytes = directory_size,
  };
}

void ApplyCdmStorageQuota(base::FilePath cdm_data_path,
                          uint64_t cdm_data_quota_bytes) {
  // TODO(crbug.com/42050202): Migrate to using a platform-provided quota
  // mechanism to manage CDM storage.
  VLOG(2) << "Enumerating CDM data directories.";

  uint64_t directories_size_bytes = 0;
  std::vector<CdmDirectoryInfo> directories_info;

  // CDM storage consistes of per-origin directories, each containing one or
  // more per-key-system sub-directories. Each per-origin-per-key-system
  // directory is assumed to be independent of other CDM data.
  base::FileEnumerator by_origin(cdm_data_path, false /* recursive */,
                                 base::FileEnumerator::DIRECTORIES);
  for (;;) {
    const base::FilePath origin_directory = by_origin.Next();
    if (origin_directory.empty()) {
      break;
    }
    base::FileEnumerator by_key_system(origin_directory, false /* recursive */,
                                       base::FileEnumerator::DIRECTORIES);
    for (;;) {
      const base::FilePath key_system_directory = by_key_system.Next();
      if (key_system_directory.empty()) {
        break;
      }
      directories_info.push_back(GetCdmDirectoryInfo(key_system_directory));
      directories_size_bytes += directories_info.back().size_bytes;
    }
  }

  if (directories_size_bytes <= cdm_data_quota_bytes) {
    return;
  }

  VLOG(1) << "Removing least recently accessed CDM data.";

  // Enumerate directories starting with the least most recently "used",
  // deleting them until the the total amount of CDM data is within quota.
  std::sort(directories_info.begin(), directories_info.end(),
            [](const CdmDirectoryInfo& lhs, const CdmDirectoryInfo& rhs) {
              return lhs.last_used < rhs.last_used;
            });
  base::flat_set<base::FilePath> affected_origin_directories;
  for (const auto& directory_info : directories_info) {
    if (directories_size_bytes <= cdm_data_quota_bytes) {
      break;
    }

    VLOG(1) << "Removing " << directory_info.path;
    base::DeletePathRecursively(directory_info.path);
    affected_origin_directories.insert(directory_info.path.DirName());

    DCHECK_GE(directories_size_bytes, directory_info.size_bytes);
    directories_size_bytes -= directory_info.size_bytes;
  }

  // Enumerate all the origin directories that had sub-directories deleted,
  // and delete any that are now empty.
  for (const auto& origin_directory : affected_origin_directories) {
    if (base::IsDirectoryEmpty(origin_directory)) {
      base::DeleteFile(origin_directory);
    }
  }
}

std::string HexEncodeHash(const std::string& name) {
  uint32_t hash = base::PersistentHash(name);
  return base::HexEncode(&hash, sizeof(uint32_t));
}

// Returns a nullopt if storage was created successfully.
std::optional<base::File::Error> CreateStorageDirectory(base::FilePath path) {
  base::File::Error error;
  bool success = base::CreateDirectoryAndGetError(path, &error);
  if (!success) {
    return error;
  }
  return {};
}

FuchsiaCdmManager* g_fuchsia_cdm_manager_instance = nullptr;

}  // namespace

// Manages individual KeySystem connections. Provides data stores and
// ProvisioningFetchers to the KeySystem server and associating CDM requests
// with the appropriate data store.
class FuchsiaCdmManager::KeySystemClient {
 public:
  // Construct an unbound KeySystemClient. The |name| field should be the EME
  // name of the key system, such as org.w3.clearkey. It is only used for
  // logging purposes.
  explicit KeySystemClient(std::string name) : name_(std::move(name)) {}
  ~KeySystemClient() = default;

  // Registers an error handler and binds the KeySystem handle. If Bind returns
  // an error, the error handler will not be called.
  zx_status_t Bind(
      fidl::InterfaceHandle<fuchsia::media::drm::KeySystem> key_system_handle,
      base::OnceClosure error_callback) {
    key_system_.set_error_handler(
        [name = name_, error_callback = std::move(error_callback)](
            zx_status_t status) mutable {
          ZX_LOG(ERROR, status) << "KeySystem " << name << " closed channel.";
          std::move(error_callback).Run();
        });

    return key_system_.Bind(std::move(key_system_handle));
  }

  void CreateCdm(
      base::FilePath storage_path,
      CreateFetcherCB create_fetcher_callback,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) {
    std::optional<DataStoreId> data_store_id = GetDataStoreIdForPath(
        std::move(storage_path), std::move(create_fetcher_callback));
    if (!data_store_id) {
      request.Close(ZX_ERR_NO_RESOURCES);
      return;
    }

    // If this request triggered an AddDataStore() request, then that will be
    // processed before this call. If AddDataStore() fails, then the
    // |data_store_id| will not be valid and the create call will close the
    // |request| with a ZX_ERR_NOT_FOUND epitaph.
    key_system_->CreateContentDecryptionModule2(data_store_id.value(),
                                                std::move(request));
  }

 private:
  using DataStoreId = uint32_t;

  std::optional<DataStoreId> GetDataStoreIdForPath(
      base::FilePath storage_path,
      CreateFetcherCB create_fetcher_callback) {
    // If we have already added a data store id for that path, just use that
    // one.
    auto it = data_store_ids_by_path_.find(storage_path);
    if (it != data_store_ids_by_path_.end()) {
      return it->second;
    }

    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory =
        base::OpenDirectoryHandle(storage_path);
    if (!data_directory.is_valid()) {
      DLOG(ERROR) << "Unable to OpenDirectory " << storage_path;
      return std::nullopt;
    }

    auto provisioning_fetcher =
        std::make_unique<FuchsiaCdmProvisioningFetcherImpl>(
            std::move(create_fetcher_callback));

    DataStoreId data_store_id = next_data_store_id_++;

    fuchsia::media::drm::DataStoreParams params;
    params.set_data_directory(std::move(data_directory));
    params.set_provisioning_fetcher(provisioning_fetcher->Bind(
        base::BindOnce(&KeySystemClient::OnProvisioningFetcherError,
                       base::Unretained(this), provisioning_fetcher.get())));

    key_system_->AddDataStore(
        data_store_id, std::move(params),
        [this, data_store_id, storage_path](
            fpromise::result<void, fuchsia::media::drm::Error> result) {
          if (result.is_error()) {
            DLOG(ERROR) << "Failed to add data store " << data_store_id
                        << ", path: " << storage_path;
            data_store_ids_by_path_.erase(storage_path);
            return;
          }
        });

    provisioning_fetchers_.insert(std::move(provisioning_fetcher));
    data_store_ids_by_path_.emplace(std::move(storage_path), data_store_id);
    return data_store_id;
  }

  void OnProvisioningFetcherError(
      FuchsiaCdmProvisioningFetcherImpl* provisioning_fetcher) {
    provisioning_fetchers_.erase(provisioning_fetcher);
  }

  // The EME name of the key system, such as org.w3.clearkey
  std::string name_;

  // FIDL InterfacePtr to the platform provided KeySystem
  fuchsia::media::drm::KeySystemPtr key_system_;

  // A set of ProvisioningFetchers, one for each data store that gets added.
  // The KeySystem might close the channel even if the data store remains in
  // use.
  base::flat_set<std::unique_ptr<FuchsiaCdmProvisioningFetcherImpl>,
                 base::UniquePtrComparator>
      provisioning_fetchers_;

  // The next data store id to use when registering data stores with the
  // KeySystem. Data store ids are scoped to the KeySystem channel. Value starts
  // at 1 because 0 is a reserved sentinel value for
  // fuchsia::media::drm::NO_DATA_STORE. The value will be incremented each time
  // we add a DataStore.
  DataStoreId next_data_store_id_ = 1;

  // A map of directory paths to data store ids that have been added to the
  // KeySystem.
  base::flat_map<base::FilePath, DataStoreId> data_store_ids_by_path_;
};

// static
FuchsiaCdmManager* FuchsiaCdmManager::GetInstance() {
  return g_fuchsia_cdm_manager_instance;
}

FuchsiaCdmManager::FuchsiaCdmManager(
    CreateKeySystemCallbackMap create_key_system_callbacks_by_name,
    base::FilePath cdm_data_path,
    std::optional<uint64_t> cdm_data_quota_bytes)
    : create_key_system_callbacks_by_name_(
          std::move(create_key_system_callbacks_by_name)),
      cdm_data_path_(std::move(cdm_data_path)),
      cdm_data_quota_bytes_(std::move(cdm_data_quota_bytes)),
      storage_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  // To avoid potential for the CDM directory "cleanup" task removing
  // CDM data directories that are in active use, the |storage_task_runner_| is
  // sequenced, thereby ensuring cleanup completes before any CDM activities
  // start.
  if (cdm_data_quota_bytes_) {
    ApplyCdmStorageQuota(cdm_data_path_, *cdm_data_quota_bytes_);
  }

  DCHECK(!g_fuchsia_cdm_manager_instance);
  g_fuchsia_cdm_manager_instance = this;
}

FuchsiaCdmManager::~FuchsiaCdmManager() {
  DCHECK_EQ(g_fuchsia_cdm_manager_instance, this);
  g_fuchsia_cdm_manager_instance = nullptr;
}

void FuchsiaCdmManager::CreateAndProvision(
    const std::string& key_system,
    const url::Origin& origin,
    CreateFetcherCB create_fetcher_cb,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::FilePath storage_path = GetStoragePath(key_system, origin);

  auto task = base::BindOnce(&CreateStorageDirectory, storage_path);
  storage_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task),
      base::BindOnce(&FuchsiaCdmManager::CreateCdm, weak_factory_.GetWeakPtr(),
                     key_system, std::move(create_fetcher_cb),
                     std::move(request), std::move(storage_path)));
}

void FuchsiaCdmManager::set_on_key_system_disconnect_for_test_callback(
    base::RepeatingCallback<void(const std::string&)> disconnect_callback) {
  on_key_system_disconnect_for_test_callback_ = std::move(disconnect_callback);
}

FuchsiaCdmManager::KeySystemClient*
FuchsiaCdmManager::GetOrCreateKeySystemClient(
    const std::string& key_system_name) {
  auto client_it = active_key_system_clients_by_name_.find(key_system_name);
  if (client_it == active_key_system_clients_by_name_.end()) {
    // If there is no active one, attempt to create one.
    return CreateKeySystemClient(key_system_name);
  }
  return client_it->second.get();
}

FuchsiaCdmManager::KeySystemClient* FuchsiaCdmManager::CreateKeySystemClient(
    const std::string& key_system_name) {
  const auto create_callback_it =
      create_key_system_callbacks_by_name_.find(key_system_name);
  if (create_callback_it == create_key_system_callbacks_by_name_.cend()) {
    DLOG(ERROR) << "Key system is not supported: " << key_system_name;
    return nullptr;
  }

  auto key_system_client = std::make_unique<KeySystemClient>(key_system_name);
  zx_status_t status = key_system_client->Bind(
      create_callback_it->second.Run(),
      base::BindOnce(&FuchsiaCdmManager::OnKeySystemClientError,
                     base::Unretained(this), key_system_name));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "Unable to bind to KeySystem";
    return nullptr;
  }

  KeySystemClient* key_system_client_ptr = key_system_client.get();
  active_key_system_clients_by_name_.emplace(key_system_name,
                                             std::move(key_system_client));
  return key_system_client_ptr;
}

base::FilePath FuchsiaCdmManager::GetStoragePath(const std::string& key_system,
                                                 const url::Origin& origin) {
  return cdm_data_path_.Append(HexEncodeHash(origin.Serialize()))
      .Append(HexEncodeHash(key_system));
}

void FuchsiaCdmManager::CreateCdm(
    const std::string& key_system_name,
    CreateFetcherCB create_fetcher_cb,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request,
    base::FilePath storage_path,
    std::optional<base::File::Error> storage_creation_error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (storage_creation_error) {
    DLOG(ERROR) << "Failed to create directory: " << storage_path
                << ", error: " << *storage_creation_error;
    request.Close(ZX_ERR_NO_RESOURCES);
    return;
  }

  KeySystemClient* key_system_client =
      GetOrCreateKeySystemClient(key_system_name);
  if (!key_system_client) {
    // GetOrCreateKeySystemClient will log the reason for failure.
    request.Close(ZX_ERR_NOT_FOUND);
    return;
  }

  key_system_client->CreateCdm(std::move(storage_path),
                               std::move(create_fetcher_cb),
                               std::move(request));
}

void FuchsiaCdmManager::OnKeySystemClientError(
    const std::string& key_system_name) {
  if (on_key_system_disconnect_for_test_callback_) {
    on_key_system_disconnect_for_test_callback_.Run(key_system_name);
  }

  active_key_system_clients_by_name_.erase(key_system_name);
}

}  // namespace media
