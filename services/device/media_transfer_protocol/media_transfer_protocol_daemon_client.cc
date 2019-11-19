// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/media_transfer_protocol/media_transfer_protocol_daemon_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "services/device/media_transfer_protocol/mtp_storage_info.pb.h"
#include "services/device/media_transfer_protocol/mtp_file_entry.pb.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace device {

namespace {

const char kInvalidResponseMsg[] = "Invalid Response: ";
uint32_t kMaxChunkSize = 1024 * 1024;  // D-Bus has message size limits.

mojom::MtpFileEntry GetMojoMtpFileEntryFromProtobuf(
    const MtpFileEntry& entry) {
  return mojom::MtpFileEntry(
      entry.item_id(),
      entry.parent_id(),
      entry.file_name(),
      entry.file_size(),
      entry.modification_time(),
      static_cast<mojom::MtpFileEntry::FileType>(entry.file_type()));
}

mojom::MtpStorageInfo GetMojoMtpStorageInfoFromProtobuf(
    const MtpStorageInfo& protobuf) {
  return mojom::MtpStorageInfo(
        protobuf.storage_name(),
        protobuf.vendor(),
        protobuf.vendor_id(),
        protobuf.product(),
        protobuf.product_id(),
        protobuf.device_flags(),
        protobuf.storage_type(),
        protobuf.filesystem_type(),
        protobuf.access_capability(),
        protobuf.max_capacity(),
        protobuf.free_space_in_bytes(),
        protobuf.free_space_in_objects(),
        protobuf.storage_description(),
        protobuf.volume_identifier());
}

// The MediaTransferProtocolDaemonClient implementation.
class MediaTransferProtocolDaemonClientImpl
    : public MediaTransferProtocolDaemonClient {
 public:
  explicit MediaTransferProtocolDaemonClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(mtpd::kMtpdServiceName,
                                   dbus::ObjectPath(mtpd::kMtpdServicePath))),
        listen_for_changes_called_(false) {}

  // MediaTransferProtocolDaemonClient override.
  void EnumerateStorages(EnumerateStoragesCallback callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kEnumerateStorages);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &MediaTransferProtocolDaemonClientImpl::OnEnumerateStorages,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void GetStorageInfo(const std::string& storage_name,
                      GetStorageInfoCallback callback,
                      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kGetStorageInfo);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(storage_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnGetStorageInfo,
                       weak_ptr_factory_.GetWeakPtr(), storage_name,
                       std::move(callback), std::move(error_callback)));
  }

  void GetStorageInfoFromDevice(const std::string& storage_name,
                                GetStorageInfoCallback callback,
                                ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kGetStorageInfoFromDevice);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(storage_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnGetStorageInfo,
                       weak_ptr_factory_.GetWeakPtr(), storage_name,
                       std::move(callback), std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void OpenStorage(const std::string& storage_name,
                   const std::string& mode,
                   OpenStorageCallback callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kOpenStorage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(storage_name);
    DCHECK(mode == mtpd::kReadOnlyMode || mode == mtpd::kReadWriteMode);
    writer.AppendString(mode);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnOpenStorage,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void CloseStorage(const std::string& handle,
                    CloseStorageCallback callback,
                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kCloseStorage);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnCloseStorage,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void CreateDirectory(const std::string& handle,
                       const uint32_t parent_id,
                       const std::string& directory_name,
                       CreateDirectoryCallback callback,
                       ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kCreateDirectory);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(parent_id);
    writer.AppendString(directory_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &MediaTransferProtocolDaemonClientImpl::OnCreateDirectory,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void ReadDirectoryEntryIds(const std::string& handle,
                             uint32_t file_id,
                             ReadDirectoryEntryIdsCallback callback,
                             ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kReadDirectoryEntryIds);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(file_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &MediaTransferProtocolDaemonClientImpl::OnReadDirectoryIds,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            std::move(error_callback)));
  }

  void GetFileInfo(const std::string& handle,
                   const std::vector<uint32_t>& file_ids,
                   GetFileInfoCallback callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kGetFileInfo);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    {
      dbus::MessageWriter array_writer(nullptr);
      writer.OpenArray("u", &array_writer);

      for (uint32_t file_id : file_ids)
        array_writer.AppendUint32(file_id);

      writer.CloseContainer(&array_writer);
    }
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnGetFileInfo,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void ReadFileChunk(const std::string& handle,
                     uint32_t file_id,
                     uint32_t offset,
                     uint32_t bytes_to_read,
                     ReadFileCallback callback,
                     ErrorCallback error_callback) override {
    DCHECK_LE(bytes_to_read, kMaxChunkSize);
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kReadFileChunk);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(file_id);
    writer.AppendUint32(offset);
    writer.AppendUint32(bytes_to_read);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnReadFile,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void RenameObject(const std::string& handle,
                    const uint32_t object_id,
                    const std::string& new_name,
                    RenameObjectCallback callback,
                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kRenameObject);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(object_id);
    writer.AppendString(new_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnRenameObject,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  void CopyFileFromLocal(const std::string& handle,
                         const int source_file_descriptor,
                         const uint32_t parent_id,
                         const std::string& file_name,
                         CopyFileFromLocalCallback callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface,
                                 mtpd::kCopyFileFromLocal);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendFileDescriptor(source_file_descriptor);
    writer.AppendUint32(parent_id);
    writer.AppendString(file_name);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(
            &MediaTransferProtocolDaemonClientImpl::OnCopyFileFromLocal,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            std::move(error_callback)));
  }

  void DeleteObject(const std::string& handle,
                    const uint32_t object_id,
                    DeleteObjectCallback callback,
                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(mtpd::kMtpdInterface, mtpd::kDeleteObject);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(handle);
    writer.AppendUint32(object_id);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&MediaTransferProtocolDaemonClientImpl::OnDeleteObject,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(error_callback)));
  }

  // MediaTransferProtocolDaemonClient override.
  void ListenForChanges(const MTPStorageEventHandler& handler) override {
    DCHECK(!listen_for_changes_called_);
    listen_for_changes_called_ = true;

    static const SignalEventTuple kSignalEventTuples[] = {
      { mtpd::kMTPStorageAttached, true },
      { mtpd::kMTPStorageDetached, false },
    };
    for (const auto& event : kSignalEventTuples) {
      proxy_->ConnectToSignal(
          mtpd::kMtpdInterface, event.signal_name,
          base::Bind(&MediaTransferProtocolDaemonClientImpl::OnMTPStorageSignal,
                     weak_ptr_factory_.GetWeakPtr(), handler, event.is_attach),
          base::Bind(&MediaTransferProtocolDaemonClientImpl::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

 private:
  // A struct to contain a pair of signal name and attachment event type.
  // Used by SetUpConnections.
  struct SignalEventTuple {
    const char* signal_name;
    bool is_attach;
  };

  // Handles the result of EnumerateStorages and calls |callback| or
  // |error_callback|.
  void OnEnumerateStorages(EnumerateStoragesCallback callback,
                           ErrorCallback error_callback,
                           dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::vector<std::string> storage_names;
    if (!reader.PopArrayOfStrings(&storage_names)) {
      LOG(ERROR) << kInvalidResponseMsg << response->ToString();
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run(storage_names);
  }

  // Handles the result of GetStorageInfo and calls |callback| or
  // |error_callback|.
  void OnGetStorageInfo(const std::string& storage_name,
                        GetStorageInfoCallback callback,
                        ErrorCallback error_callback,
                        dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    dbus::MessageReader reader(response);
    MtpStorageInfo protobuf;
    if (!reader.PopArrayOfBytesAsProto(&protobuf)) {
      LOG(ERROR) << kInvalidResponseMsg << response->ToString();
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run(GetMojoMtpStorageInfoFromProtobuf(protobuf));
  }

  // Handles the result of OpenStorage and calls |callback| or |error_callback|.
  void OnOpenStorage(OpenStorageCallback callback,
                     ErrorCallback error_callback,
                     dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    dbus::MessageReader reader(response);
    std::string handle;
    if (!reader.PopString(&handle)) {
      LOG(ERROR) << kInvalidResponseMsg << response->ToString();
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run(handle);
  }

  // Handles the result of CloseStorage and calls |callback| or
  // |error_callback|.
  void OnCloseStorage(CloseStorageCallback callback,
                      ErrorCallback error_callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run();
  }

  void OnCreateDirectory(CreateDirectoryCallback callback,
                         ErrorCallback error_callback,
                         dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }
    std::move(callback).Run();
  }

  // Handles the result of ReadDirectoryEntryIds and calls |callback| or
  // |error_callback|.
  void OnReadDirectoryIds(ReadDirectoryEntryIdsCallback callback,
                          ErrorCallback error_callback,
                          dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    dbus::MessageReader reader(response);
    dbus::MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader) || reader.HasMoreData()) {
      LOG(ERROR) << kInvalidResponseMsg << response->ToString();
      std::move(error_callback).Run();
      return;
    }

    std::vector<uint32_t> file_ids;
    while (array_reader.HasMoreData()) {
      uint32_t file_id;
      if (!array_reader.PopUint32(&file_id)) {
        LOG(ERROR) << kInvalidResponseMsg << response->ToString();
        std::move(error_callback).Run();
        return;
      }
      file_ids.push_back(file_id);
    }
    std::move(callback).Run(file_ids);
  }

  // Handles the result of GetFileInfo and calls |callback| or |error_callback|.
  void OnGetFileInfo(GetFileInfoCallback callback,
                     ErrorCallback error_callback,
                     dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    dbus::MessageReader reader(response);
    MtpFileEntries entries_protobuf;
    if (!reader.PopArrayOfBytesAsProto(&entries_protobuf)) {
      LOG(ERROR) << kInvalidResponseMsg << response->ToString();
      std::move(error_callback).Run();
      return;
    }

    std::vector<mojom::MtpFileEntry> file_entries;
    file_entries.reserve(entries_protobuf.file_entries_size());
    for (int i = 0; i < entries_protobuf.file_entries_size(); ++i) {
      const auto& entry = entries_protobuf.file_entries(i);
      file_entries.push_back(
          GetMojoMtpFileEntryFromProtobuf(entry));
    }
    std::move(callback).Run(file_entries);
  }

  // Handles the result of ReadFileChunk and calls |callback| or
  // |error_callback|.
  void OnReadFile(ReadFileCallback callback,
                  ErrorCallback error_callback,
                  dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    const uint8_t* data_bytes = nullptr;
    size_t data_length = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopArrayOfBytes(&data_bytes, &data_length)) {
      std::move(error_callback).Run();
      return;
    }
    std::string data(reinterpret_cast<const char*>(data_bytes), data_length);
    std::move(callback).Run(data);
  }

  void OnRenameObject(RenameObjectCallback callback,
                      ErrorCallback error_callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run();
  }

  void OnCopyFileFromLocal(CopyFileFromLocalCallback callback,
                           ErrorCallback error_callback,
                           dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run();
  }

  void OnDeleteObject(DeleteObjectCallback callback,
                      ErrorCallback error_callback,
                      dbus::Response* response) {
    if (!response) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run();
  }

  // Handles MTPStorageAttached/Dettached signals and calls |handler|.
  void OnMTPStorageSignal(MTPStorageEventHandler handler,
                          bool is_attach,
                          dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string storage_name;
    if (!reader.PopString(&storage_name)) {
      LOG(ERROR) << "Invalid signal: " << signal->ToString();
      return;
    }
    DCHECK(!storage_name.empty());
    handler.Run(is_attach, storage_name);
  }


  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " "
                              << signal << " failed.";
  }

  dbus::ObjectProxy* const proxy_;

  bool listen_for_changes_called_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MediaTransferProtocolDaemonClientImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDaemonClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MediaTransferProtocolDaemonClient

MediaTransferProtocolDaemonClient::MediaTransferProtocolDaemonClient() =
    default;

MediaTransferProtocolDaemonClient::~MediaTransferProtocolDaemonClient() =
    default;

// static
std::unique_ptr<MediaTransferProtocolDaemonClient>
MediaTransferProtocolDaemonClient::Create(dbus::Bus* bus) {
  return std::make_unique<MediaTransferProtocolDaemonClientImpl>(bus);
}

}  // namespace device
