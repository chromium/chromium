// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/media_transfer_protocol/mtp_device_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "dbus/bus.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

namespace {

#if DCHECK_IS_ON()
MtpDeviceManager* g_mtp_device_manager = nullptr;
#endif

}  // namespace

MtpDeviceManager::MtpDeviceManager()
    : bus_(ash::DBusThreadManager::Get()->GetSystemBus()) {
  // Listen for future mtpd service owner changes, in case it is not
  // available right now. There is no guarantee that mtpd is running already.
  dbus::Bus::ServiceOwnerChangeCallback mtpd_owner_changed_callback =
      base::BindRepeating(&MtpDeviceManager::FinishSetupOnOriginThread,
                          weak_ptr_factory_.GetWeakPtr());
  if (bus_) {
    bus_->ListenForServiceOwnerChange(mtpd::kMtpdServiceName,
                                      mtpd_owner_changed_callback);
    bus_->GetServiceOwner(mtpd::kMtpdServiceName, mtpd_owner_changed_callback);
  }
}

MtpDeviceManager::~MtpDeviceManager() {
#if DCHECK_IS_ON()
  DCHECK(g_mtp_device_manager);
  g_mtp_device_manager = nullptr;
#endif

  if (bus_) {
    bus_->UnlistenForServiceOwnerChange(
        mtpd::kMtpdServiceName,
        base::BindRepeating(&MtpDeviceManager::FinishSetupOnOriginThread,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  VLOG(1) << "MtpDeviceManager Shutdown completed";
}

void MtpDeviceManager::AddReceiver(
    mojo::PendingReceiver<mojom::MtpManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MtpDeviceManager::EnumerateStoragesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::MtpManagerClient> client,
    EnumerateStoragesAndSetClientCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Return all available storage info.
  std::vector<mojom::MtpStorageInfoPtr> storage_info_ptr_list;
  storage_info_ptr_list.reserve(storage_info_map_.size());
  for (const auto& info : storage_info_map_)
    storage_info_ptr_list.push_back(info.second.Clone());
  std::move(callback).Run(std::move(storage_info_ptr_list));

  // Set client.
  if (!client.is_valid())
    return;

  client_.Bind(std::move(client));
}

void MtpDeviceManager::GetStorageInfo(const std::string& storage_name,
                                      GetStorageInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto it = storage_info_map_.find(storage_name);
  mojom::MtpStorageInfoPtr storage_info =
      it != storage_info_map_.end() ? it->second.Clone() : nullptr;
  std::move(callback).Run(std::move(storage_info));
}

void MtpDeviceManager::GetStorageInfoFromDevice(
    const std::string& storage_name,
    GetStorageInfoFromDeviceCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(storage_info_map_, storage_name) || !mtp_client_) {
    std::move(callback).Run(nullptr, true /* error */);
    return;
  }
  get_storage_info_from_device_callbacks_.push(std::move(callback));
  mtp_client_->GetStorageInfoFromDevice(
      storage_name,
      base::BindOnce(&MtpDeviceManager::OnGetStorageInfoFromDevice,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnGetStorageInfoFromDeviceError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::OpenStorage(const std::string& storage_name,
                                   const std::string& mode,
                                   OpenStorageCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(storage_info_map_, storage_name) || !mtp_client_) {
    std::move(callback).Run(std::string(), true);
    return;
  }
  open_storage_callbacks_.push(std::move(callback));
  mtp_client_->OpenStorage(storage_name, mode,
                           base::BindOnce(&MtpDeviceManager::OnOpenStorage,
                                          weak_ptr_factory_.GetWeakPtr()),
                           base::BindOnce(&MtpDeviceManager::OnOpenStorageError,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::CloseStorage(const std::string& storage_handle,
                                    CloseStorageCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(true);
    return;
  }
  close_storage_callbacks_.push(
      std::make_pair(std::move(callback), storage_handle));
  mtp_client_->CloseStorage(
      storage_handle,
      base::BindOnce(&MtpDeviceManager::OnCloseStorage,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnCloseStorageError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::CreateDirectory(const std::string& storage_handle,
                                       uint32_t parent_id,
                                       const std::string& directory_name,
                                       CreateDirectoryCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(true /* error */);
    return;
  }
  create_directory_callbacks_.push(std::move(callback));
  mtp_client_->CreateDirectory(
      storage_handle, parent_id, directory_name,
      base::BindOnce(&MtpDeviceManager::OnCreateDirectory,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnCreateDirectoryError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::ReadDirectoryEntryIds(
    const std::string& storage_handle,
    uint32_t file_id,
    ReadDirectoryEntryIdsCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(std::vector<uint32_t>(), /*error=*/true);
    return;
  }
  read_directory_callbacks_.push(std::move(callback));
  mtp_client_->ReadDirectoryEntryIds(
      storage_handle, file_id,
      base::BindOnce(&MtpDeviceManager::OnReadDirectoryEntryIds,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnReadDirectoryError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::ReadFileChunk(const std::string& storage_handle,
                                     uint32_t file_id,
                                     uint32_t offset,
                                     uint32_t count,
                                     ReadFileChunkCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(std::string(), true);
    return;
  }
  read_file_callbacks_.push(std::move(callback));
  mtp_client_->ReadFileChunk(storage_handle, file_id, offset, count,
                             base::BindOnce(&MtpDeviceManager::OnReadFile,
                                            weak_ptr_factory_.GetWeakPtr()),
                             base::BindOnce(&MtpDeviceManager::OnReadFileError,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::GetFileInfo(const std::string& storage_handle,
                                   const std::vector<uint32_t>& file_ids,
                                   GetFileInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(std::vector<device::mojom::MtpFileEntryPtr>(),
                            /*error=*/true);
    return;
  }
  get_file_info_callbacks_.push(std::move(callback));
  mtp_client_->GetFileInfo(storage_handle, file_ids,
                           base::BindOnce(&MtpDeviceManager::OnGetFileInfo,
                                          weak_ptr_factory_.GetWeakPtr()),
                           base::BindOnce(&MtpDeviceManager::OnGetFileInfoError,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::RenameObject(const std::string& storage_handle,
                                    uint32_t object_id,
                                    const std::string& new_name,
                                    RenameObjectCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(true /* error */);
    return;
  }
  rename_object_callbacks_.push(std::move(callback));
  mtp_client_->RenameObject(
      storage_handle, object_id, new_name,
      base::BindOnce(&MtpDeviceManager::OnRenameObject,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnRenameObjectError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::CopyFileFromLocal(const std::string& storage_handle,
                                         int64_t source_file_descriptor,
                                         uint32_t parent_id,
                                         const std::string& file_name,
                                         CopyFileFromLocalCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(true /* error */);
    return;
  }
  copy_file_from_local_callbacks_.push(std::move(callback));
  mtp_client_->CopyFileFromLocal(
      storage_handle, source_file_descriptor, parent_id, file_name,
      base::BindOnce(&MtpDeviceManager::OnCopyFileFromLocal,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnCopyFileFromLocalError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MtpDeviceManager::DeleteObject(const std::string& storage_handle,
                                    uint32_t object_id,
                                    DeleteObjectCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, storage_handle) || !mtp_client_) {
    std::move(callback).Run(true /* error */);
    return;
  }
  delete_object_callbacks_.push(std::move(callback));
  mtp_client_->DeleteObject(
      storage_handle, object_id,
      base::BindOnce(&MtpDeviceManager::OnDeleteObject,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MtpDeviceManager::OnDeleteObjectError,
                     weak_ptr_factory_.GetWeakPtr()));
}

// private methods
void MtpDeviceManager::OnStorageAttached(const std::string& storage_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  mtp_client_->GetStorageInfo(
      storage_name,
      base::BindOnce(&MtpDeviceManager::OnGetStorageInfo,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void MtpDeviceManager::OnStorageDetached(const std::string& storage_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (storage_info_map_.erase(storage_name) == 0) {
    // This can happen for a storage where
    // MediaTransferProtocolDaemonClient::GetStorageInfo() failed.
    // Return to avoid giving client phantom detach events.
    return;
  }

  // Notify the bound MtpManagerClient.
  if (client_) {
    client_->StorageDetached(storage_name);
  }
}

void MtpDeviceManager::OnStorageChanged(bool is_attach,
                                        const std::string& storage_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(mtp_client_);
  if (is_attach)
    OnStorageAttached(storage_name);
  else
    OnStorageDetached(storage_name);
}

void MtpDeviceManager::OnEnumerateStorages(
    const std::vector<std::string>& storage_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(mtp_client_);
  for (const auto& name : storage_names) {
    if (base::Contains(storage_info_map_, name)) {
      // OnStorageChanged() might have gotten called first.
      continue;
    }
    OnStorageAttached(name);
  }
}

void MtpDeviceManager::OnGetStorageInfo(
    const mojom::MtpStorageInfo& storage_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const std::string& storage_name = storage_info.storage_name;
  if (base::Contains(storage_info_map_, storage_name)) {
    // This should not happen, since MtpDeviceManager should
    // only call EnumerateStorages() once, which populates |storage_info_map_|
    // with the already-attached devices.
    // After that, all incoming signals are either for new storage
    // attachments, which should not be in |storage_info_map_|, or for
    // storage detachments, which do not add to |storage_info_map_|.
    // Return to avoid giving client phantom detach events.
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // New storage. Add it and let the bound client know.
  storage_info_map_.insert(std::make_pair(storage_name, storage_info));

  if (client_) {
    client_->StorageAttached(storage_info.Clone());
  }
}

void MtpDeviceManager::OnGetStorageInfoFromDevice(
    const mojom::MtpStorageInfo& storage_info) {
  std::move(get_storage_info_from_device_callbacks_.front())
      .Run(storage_info.Clone(), false /* no error */);
  get_storage_info_from_device_callbacks_.pop();
}

void MtpDeviceManager::OnGetStorageInfoFromDeviceError() {
  std::move(get_storage_info_from_device_callbacks_.front())
      .Run(nullptr, true /* error */);
  get_storage_info_from_device_callbacks_.pop();
}

void MtpDeviceManager::OnOpenStorage(const std::string& handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!base::Contains(handles_, handle)) {
    handles_.insert(handle);
    std::move(open_storage_callbacks_.front()).Run(handle, false);
  } else {
    NOTREACHED_IN_MIGRATION();
    std::move(open_storage_callbacks_.front()).Run(std::string(), true);
  }
  open_storage_callbacks_.pop();
}

void MtpDeviceManager::OnOpenStorageError() {
  std::move(open_storage_callbacks_.front()).Run(std::string(), true);
  open_storage_callbacks_.pop();
}

void MtpDeviceManager::OnCloseStorage() {
  DCHECK(thread_checker_.CalledOnValidThread());
  const std::string& handle = close_storage_callbacks_.front().second;
  if (base::Contains(handles_, handle)) {
    handles_.erase(handle);
    std::move(close_storage_callbacks_.front().first).Run(false);
  } else {
    NOTREACHED_IN_MIGRATION();
    std::move(close_storage_callbacks_.front().first).Run(true);
  }
  close_storage_callbacks_.pop();
}

void MtpDeviceManager::OnCloseStorageError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(close_storage_callbacks_.front()).first.Run(true);
  close_storage_callbacks_.pop();
}

void MtpDeviceManager::OnCreateDirectory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(create_directory_callbacks_.front()).Run(false /* no error */);
  create_directory_callbacks_.pop();
}

void MtpDeviceManager::OnCreateDirectoryError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(create_directory_callbacks_.front()).Run(true /* error */);
  create_directory_callbacks_.pop();
}

void MtpDeviceManager::OnReadDirectoryEntryIds(
    const std::vector<uint32_t>& file_ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(read_directory_callbacks_.front()).Run(file_ids, /*error=*/false);
  read_directory_callbacks_.pop();
}

void MtpDeviceManager::OnReadDirectoryError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(read_directory_callbacks_.front())
      .Run(std::vector<uint32_t>(), /*error=*/true);
  read_directory_callbacks_.pop();
}

void MtpDeviceManager::OnReadFile(const std::string& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(read_file_callbacks_.front()).Run(data, false);
  read_file_callbacks_.pop();
}

void MtpDeviceManager::OnReadFileError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(read_file_callbacks_.front()).Run(std::string(), true);
  read_file_callbacks_.pop();
}

void MtpDeviceManager::OnGetFileInfo(
    const std::vector<mojom::MtpFileEntry>& entries) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<device::mojom::MtpFileEntryPtr> ret(entries.size());
  for (size_t i = 0; i < entries.size(); ++i)
    ret[i] = entries[i].Clone();
  std::move(get_file_info_callbacks_.front())
      .Run(std::move(ret), /*error=*/false);
  get_file_info_callbacks_.pop();
}

void MtpDeviceManager::OnGetFileInfoError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(get_file_info_callbacks_.front())
      .Run(std::vector<device::mojom::MtpFileEntryPtr>(), /*error=*/true);
  get_file_info_callbacks_.pop();
}

void MtpDeviceManager::OnRenameObject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(rename_object_callbacks_.front()).Run(false /* no error */);
  rename_object_callbacks_.pop();
}

void MtpDeviceManager::OnRenameObjectError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(rename_object_callbacks_.front()).Run(true /* error */);
  rename_object_callbacks_.pop();
}

void MtpDeviceManager::OnCopyFileFromLocal() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(copy_file_from_local_callbacks_.front()).Run(false /* no error */);
  copy_file_from_local_callbacks_.pop();
}

void MtpDeviceManager::OnCopyFileFromLocalError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(copy_file_from_local_callbacks_.front()).Run(true /* error */);
  copy_file_from_local_callbacks_.pop();
}

void MtpDeviceManager::OnDeleteObject() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(delete_object_callbacks_.front()).Run(false /* no error */);
  delete_object_callbacks_.pop();
}

void MtpDeviceManager::OnDeleteObjectError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::move(delete_object_callbacks_.front()).Run(true /* error */);
  delete_object_callbacks_.pop();
}

void MtpDeviceManager::FinishSetupOnOriginThread(
    const std::string& mtpd_service_owner) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (mtpd_service_owner == current_mtpd_owner_)
    return;

  // In the case of a new service owner, clear |storage_info_map_|.
  // Assume all storages have been disconnected. If there is a new service
  // owner, reconnecting to it will reconnect all the storages as well.

  // Save a copy of |storage_info_map_| keys as |storage_info_map_| can
  // change in OnStorageDetached().
  std::vector<std::string> storage_names;
  storage_names.reserve(storage_info_map_.size());
  for (const auto& info : storage_info_map_)
    storage_names.push_back(info.first);

  for (const auto& name : storage_names)
    OnStorageDetached(name);

  if (mtpd_service_owner.empty()) {
    current_mtpd_owner_.clear();
    mtp_client_.reset();
    return;
  }

  current_mtpd_owner_ = mtpd_service_owner;

  // |bus_| must be valid here. Otherwise, how did this method get called as a
  // callback in the first place?
  DCHECK(bus_);
  mtp_client_ = MediaTransferProtocolDaemonClient::Create(bus_.get());

  // Set up signals and start initializing |storage_info_map_|.
  mtp_client_->ListenForChanges(base::BindRepeating(
      &MtpDeviceManager::OnStorageChanged, weak_ptr_factory_.GetWeakPtr()));
  mtp_client_->EnumerateStorages(
      base::BindOnce(&MtpDeviceManager::OnEnumerateStorages,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

// static
std::unique_ptr<MtpDeviceManager> MtpDeviceManager::Initialize() {
  auto manager = std::make_unique<MtpDeviceManager>();

  VLOG(1) << "MtpDeviceManager initialized";

#if DCHECK_IS_ON()
  DCHECK(!g_mtp_device_manager);
  g_mtp_device_manager = manager.get();
#endif

  return manager;
}

}  // namespace device
