// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_FUCHSIA_CDM_MANAGER_H_
#define MEDIA_MOJO_SERVICES_FUCHSIA_CDM_MANAGER_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/base/provision_fetcher.h"
#include "media/mojo/services/media_mojo_export.h"

namespace url {
class Origin;
}  // namespace url

namespace media {

// Create and connect to Fuchsia CDM services. Additionally manages the storage
// for CDM user data.
class MEDIA_MOJO_EXPORT FuchsiaCdmManager {
 public:
  using CreateKeySystemCallback = base::RepeatingCallback<
      fidl::InterfaceHandle<fuchsia::media::drm::KeySystem>()>;

  // A map from key system name to its CreateKeySystemCallback.
  using CreateKeySystemCallbackMap =
      base::flat_map<std::string, CreateKeySystemCallback>;

  static FuchsiaCdmManager* GetInstance();

  // |cdm_data_quota_bytes| is currently only applied once, when the manager is
  // created.
  FuchsiaCdmManager(
      CreateKeySystemCallbackMap create_key_system_callbacks_by_name,
      base::FilePath cdm_data_path,
      std::optional<uint64_t> cdm_data_quota_bytes);

  ~FuchsiaCdmManager();

  FuchsiaCdmManager(FuchsiaCdmManager&&) = delete;
  FuchsiaCdmManager& operator=(FuchsiaCdmManager&&) = delete;

  void CreateAndProvision(
      const std::string& key_system,
      const url::Origin& origin,
      CreateFetcherCB create_fetcher_cb,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request);

  // Used by tests to monitor for key system disconnection events. The key
  // system name is passed as a parameter to the callback.
  void set_on_key_system_disconnect_for_test_callback(
      base::RepeatingCallback<void(const std::string&)> disconnect_callback);

 private:
  class KeySystemClient;
  using KeySystemClientMap =
      base::flat_map<std::string, std::unique_ptr<KeySystemClient>>;

  KeySystemClient* GetOrCreateKeySystemClient(
      const std::string& key_system_name);
  KeySystemClient* CreateKeySystemClient(const std::string& key_system_name);
  base::FilePath GetStoragePath(const std::string& key_system_name,
                                const url::Origin& origin);
  void CreateCdm(
      const std::string& key_system_name,
      CreateFetcherCB create_fetcher_cb,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request,
      base::FilePath storage_path,
      std::optional<base::File::Error> storage_creation_error);
  void OnKeySystemClientError(const std::string& key_system_name);

  // A map of callbacks to create KeySystem channels indexed by their EME name.
  const CreateKeySystemCallbackMap create_key_system_callbacks_by_name_;
  const base::FilePath cdm_data_path_;
  const std::optional<uint64_t> cdm_data_quota_bytes_;

  // Used for operations on the CDM data directory.
  const scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;

  // A map of the active KeySystem clients indexed by their EME name.  Entries
  // in this map will be added on the first CreateAndProvision call for that
  // particular KeySystem. They will only be removed if the KeySystem channel
  // receives an error.
  KeySystemClientMap active_key_system_clients_by_name_;

  base::RepeatingCallback<void(const std::string&)>
      on_key_system_disconnect_for_test_callback_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<FuchsiaCdmManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_FUCHSIA_CDM_MANAGER_H_
