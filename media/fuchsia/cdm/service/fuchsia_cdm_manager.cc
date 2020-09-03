// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/fuchsia/cdm/service/provisioning_fetcher_impl.h"
#include "url/origin.h"

namespace media {

namespace {

std::string HexEncodeHash(const std::string& name) {
  uint32_t hash = base::PersistentHash(name);
  return base::HexEncode(&hash, sizeof(uint32_t));
}

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
    base::Optional<DataStoreId> data_store_id = GetDataStoreIdForPath(
        std::move(storage_path), std::move(create_fetcher_callback));
    if (!data_store_id) {
      DLOG(ERROR) << "Unable to create DataStore for path: " << storage_path;
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

  base::Optional<DataStoreId> GetDataStoreIdForPath(
      base::FilePath storage_path,
      CreateFetcherCB create_fetcher_callback) {
    // If we have already added a data store id for that path, just use that
    // one.
    auto it = data_store_ids_by_path_.find(storage_path);
    if (it != data_store_ids_by_path_.end()) {
      return it->second;
    }

    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory =
        base::fuchsia::OpenDirectory(storage_path);
    if (!data_directory.is_valid()) {
      DLOG(ERROR) << "Unable to OpenDirectory " << storage_path;
      return base::nullopt;
    }

    auto provisioning_fetcher = std::make_unique<ProvisioningFetcherImpl>(
        std::move(create_fetcher_callback));

    DataStoreId data_store_id = next_data_store_id_++;

    fuchsia::media::drm::DataStoreParams params;
    params.set_data_directory(std::move(data_directory));
    params.set_provisioning_fetcher(provisioning_fetcher->Bind(
        base::BindOnce(&KeySystemClient::OnProvisioningFetcherError,
                       base::Unretained(this), provisioning_fetcher.get())));

    key_system_->AddDataStore(
        data_store_id, std::move(params),
        [this, data_store_id,
         storage_path](fit::result<void, fuchsia::media::drm::Error> result) {
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
      ProvisioningFetcherImpl* provisioning_fetcher) {
    provisioning_fetchers_.erase(provisioning_fetcher);
  }

  // The EME name of the key system, such as org.w3.clearkey
  std::string name_;

  // FIDL InterfacePtr to the platform provided KeySystem
  fuchsia::media::drm::KeySystemPtr key_system_;

  // A set of ProvisioningFetchers, one for each data store that gets added.
  // The KeySystem might close the channel even if the data store remains in
  // use.
  base::flat_set<std::unique_ptr<ProvisioningFetcherImpl>,
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

FuchsiaCdmManager::FuchsiaCdmManager(
    CreateKeySystemCallbackMap create_key_system_callbacks_by_name,
    base::FilePath cdm_data_path)
    : create_key_system_callbacks_by_name_(
          std::move(create_key_system_callbacks_by_name)),
      cdm_data_path_(std::move(cdm_data_path)) {}

FuchsiaCdmManager::~FuchsiaCdmManager() = default;

void FuchsiaCdmManager::CreateAndProvision(
    const std::string& key_system,
    const url::Origin& origin,
    CreateFetcherCB create_fetcher_cb,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  KeySystemClient* key_system_client = GetOrCreateKeySystemClient(key_system);
  if (!key_system_client) {
    // GetOrCreateKeySystemClient will log the reason for failure.
    return;
  }

  base::FilePath storage_path = GetStoragePath(key_system, origin);
  base::File::Error error;
  bool success = base::CreateDirectoryAndGetError(storage_path, &error);
  if (!success) {
    DLOG(ERROR) << "Failed to create directory: " << storage_path
                << ", error: " << error;
    return;
  }

  key_system_client->CreateCdm(std::move(storage_path),
                               std::move(create_fetcher_cb),
                               std::move(request));
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

void FuchsiaCdmManager::OnKeySystemClientError(
    const std::string& key_system_name) {
  if (on_key_system_disconnect_for_test_callback_) {
    on_key_system_disconnect_for_test_callback_.Run(key_system_name);
  }

  active_key_system_clients_by_name_.erase(key_system_name);
}

}  // namespace media
