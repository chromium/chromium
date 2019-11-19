// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_
#define SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/media_transfer_protocol/media_transfer_protocol_daemon_client.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"

#if !defined(OS_CHROMEOS)
#error "Only used on ChromeOS"
#endif

namespace dbus {
class DBus;
}

namespace device {

// This is the implementation of device::mojom::MtpManager which provides
// various methods to get information of MTP (Media Transfer Protocol) devices.
class MtpDeviceManager : public mojom::MtpManager {
 public:
  MtpDeviceManager();
  ~MtpDeviceManager() override;

  void AddReceiver(mojo::PendingReceiver<mojom::MtpManager> receiver);

  // Implements mojom::MtpManager.
  void EnumerateStoragesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::MtpManagerClient> client,
      EnumerateStoragesAndSetClientCallback callback) override;
  void GetStorageInfo(const std::string& storage_name,
                      GetStorageInfoCallback callback) override;
  void GetStorageInfoFromDevice(
      const std::string& storage_name,
      GetStorageInfoFromDeviceCallback callback) override;
  void OpenStorage(const std::string& storage_name,
                   const std::string& mode,
                   OpenStorageCallback callback) override;
  void CloseStorage(const std::string& storage_handle,
                    CloseStorageCallback callback) override;
  void CreateDirectory(const std::string& storage_handle,
                       uint32_t parent_id,
                       const std::string& directory_name,
                       CreateDirectoryCallback callback) override;
  void ReadDirectoryEntryIds(const std::string& storage_handle,
                             uint32_t file_id,
                             ReadDirectoryEntryIdsCallback callback) override;
  void ReadFileChunk(const std::string& storage_handle,
                     uint32_t file_id,
                     uint32_t offset,
                     uint32_t count,
                     ReadFileChunkCallback callback) override;
  void GetFileInfo(const std::string& storage_handle,
                   const std::vector<uint32_t>& file_ids,
                   GetFileInfoCallback callback) override;
  void RenameObject(const std::string& storage_handle,
                    uint32_t object_id,
                    const std::string& new_name,
                    RenameObjectCallback callback) override;
  void CopyFileFromLocal(const std::string& storage_handle,
                         int64_t source_file_descriptor,
                         uint32_t parent_id,
                         const std::string& file_name,
                         CopyFileFromLocalCallback callback) override;
  void DeleteObject(const std::string& storage_handle,
                    uint32_t object_id,
                    DeleteObjectCallback callback) override;

  // Creates and returns the global MtpDeviceManager instance.
  static std::unique_ptr<MtpDeviceManager> Initialize();

 private:
  // Map of storage names to storage info.
  using GetStorageInfoFromDeviceCallbackQueue =
      base::queue<GetStorageInfoFromDeviceCallback>;
  // Callback queues - DBus communication is in-order, thus callbacks are
  // received in the same order as the requests.
  using OpenStorageCallbackQueue = base::queue<OpenStorageCallback>;
  // (callback, handle)
  using CloseStorageCallbackQueue =
      base::queue<std::pair<CloseStorageCallback, std::string>>;
  using CreateDirectoryCallbackQueue = base::queue<CreateDirectoryCallback>;
  using ReadDirectoryCallbackQueue = base::queue<ReadDirectoryEntryIdsCallback>;
  using ReadFileCallbackQueue = base::queue<ReadFileChunkCallback>;
  using GetFileInfoCallbackQueue = base::queue<GetFileInfoCallback>;
  using RenameObjectCallbackQueue = base::queue<RenameObjectCallback>;
  using CopyFileFromLocalCallbackQueue = base::queue<CopyFileFromLocalCallback>;
  using DeleteObjectCallbackQueue = base::queue<DeleteObjectCallback>;

  void OnStorageAttached(const std::string& storage_name);

  void OnStorageDetached(const std::string& storage_name);

  void OnStorageChanged(bool is_attach, const std::string& storage_name);

  void OnEnumerateStorages(const std::vector<std::string>& storage_names);

  void OnGetStorageInfo(const mojom::MtpStorageInfo& storage_info);

  void OnGetStorageInfoFromDevice(const mojom::MtpStorageInfo& storage_info);

  void OnGetStorageInfoFromDeviceError();

  void OnOpenStorage(const std::string& handle);

  void OnOpenStorageError();

  void OnCloseStorage();

  void OnCloseStorageError();

  void OnCreateDirectory();

  void OnCreateDirectoryError();

  void OnReadDirectoryEntryIds(const std::vector<uint32_t>& file_ids);

  void OnReadDirectoryError();

  void OnReadFile(const std::string& data);

  void OnReadFileError();

  void OnGetFileInfo(const std::vector<mojom::MtpFileEntry>& entries);

  void OnGetFileInfoError();

  void OnRenameObject();

  void OnRenameObjectError();

  void OnCopyFileFromLocal();

  void OnCopyFileFromLocalError();

  void OnDeleteObject();

  void OnDeleteObjectError();

  // Callback to finish initialization after figuring out if the mtpd service
  // has an owner, or if the service owner has changed.
  // |mtpd_service_owner| contains the name of the current owner, if any.
  void FinishSetupOnOriginThread(const std::string& mtpd_service_owner);

  // Mtpd DBus client.
  std::unique_ptr<MediaTransferProtocolDaemonClient> mtp_client_;

  // And a D-Bus session for talking to mtpd. Note: In production, this is never
  // a nullptr, but in tests it oftentimes is. It may be too much work for
  // DBusThreadManager to provide a bus in unit tests.
  scoped_refptr<dbus::Bus> const bus_;

  mojo::ReceiverSet<mojom::MtpManager> receivers_;
  // MtpManager client who keeps tuned on attachment / detachment events.
  // Currently, storage_monitor::StorageMonitorCros is supposed to be the
  // only client.
  mojo::AssociatedRemote<mojom::MtpManagerClient> client_;

  // Map to keep track of attached storages by name.
  base::flat_map<std::string, mojom::MtpStorageInfo> storage_info_map_;

  // Set of open storage handles.
  base::flat_set<std::string> handles_;

  std::string current_mtpd_owner_;

  // Queued callbacks.
  // These queues are needed becasue MediaTransferProtocolDaemonClient provides
  // different callbacks for result(success_callback, error_callback) with
  // MediaTransferProtocolManager, so a passed callback for a method in this
  // class will be referred in both success_callback and error_callback for
  // underline MediaTransferProtocolDaemonClient, and it is also the case for
  // mojom interfaces, as all mojom methods are defined as OnceCallback.
  GetStorageInfoFromDeviceCallbackQueue get_storage_info_from_device_callbacks_;
  OpenStorageCallbackQueue open_storage_callbacks_;
  CloseStorageCallbackQueue close_storage_callbacks_;
  CreateDirectoryCallbackQueue create_directory_callbacks_;
  ReadDirectoryCallbackQueue read_directory_callbacks_;
  ReadFileCallbackQueue read_file_callbacks_;
  GetFileInfoCallbackQueue get_file_info_callbacks_;
  RenameObjectCallbackQueue rename_object_callbacks_;
  CopyFileFromLocalCallbackQueue copy_file_from_local_callbacks_;
  DeleteObjectCallbackQueue delete_object_callbacks_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<MtpDeviceManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MtpDeviceManager);
};

}  // namespace device

#endif  // SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_
