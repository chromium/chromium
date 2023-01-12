// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client code to talk to the Media Transfer Protocol daemon. The MTP daemon is
// responsible for communicating with PTP / MTP capable devices like cameras
// and smartphones.

#ifndef SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
#define SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/device/public/mojom/mtp_file_entry.mojom.h"
#include "services/device/public/mojom/mtp_storage_info.mojom.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error "Only used on ChromeOS"
#endif

namespace dbus {
class Bus;
}

namespace device {

// A class to make the actual DBus calls for mtpd service.
// This class only makes calls, result/error handling should be done
// by callbacks.
class MediaTransferProtocolDaemonClient {
 public:
  // A callback to be called when DBus method call fails.
  using ErrorCallback = base::OnceClosure;

  // A callback to handle the result of EnumerateStorages.
  // The argument is the enumerated storage names.
  using EnumerateStoragesCallback =
      base::OnceCallback<void(const std::vector<std::string>& storage_names)>;

  // A callback to handle the result of GetStorageInfo.
  // The argument is the information about the specified storage.
  using GetStorageInfoCallback =
      base::OnceCallback<void(const mojom::MtpStorageInfo& storage_info)>;

  // A callback to handle the result of OpenStorage.
  // The argument is the returned handle.
  using OpenStorageCallback =
      base::OnceCallback<void(const std::string& handle)>;

  // A callback to handle the result of CloseStorage.
  using CloseStorageCallback = base::OnceClosure;

  // A callback to handle the result of CreateDirectory.
  using CreateDirectoryCallback = base::OnceClosure;

  // A callback to handle the result of ReadDirectoryEntryIds.
  // The argument is a vector of file ids.
  using ReadDirectoryEntryIdsCallback =
      base::OnceCallback<void(const std::vector<uint32_t>& file_ids)>;

  // A callback to handle the result of GetFileInfo.
  // The argument is a vector of file entries.
  using GetFileInfoCallback = base::OnceCallback<void(
      const std::vector<mojom::MtpFileEntry>& file_entries)>;

  // A callback to handle the result of ReadFileChunkById.
  // The argument is a string containing the file data.
  using ReadFileCallback = base::OnceCallback<void(const std::string& data)>;

  // A callback to handle the result of RenameObject.
  using RenameObjectCallback = base::OnceClosure;

  // A callback to handle the result of CopyFileFromLocal.
  using CopyFileFromLocalCallback = base::OnceClosure;

  // A callback to handle the result of DeleteObject.
  using DeleteObjectCallback = base::OnceClosure;

  // A callback to handle storage attach/detach events.
  // The first argument is true for attach, false for detach.
  // The second argument is the storage name.
  using MTPStorageEventHandler =
      base::RepeatingCallback<void(bool is_attach,
                                   const std::string& storage_name)>;

  MediaTransferProtocolDaemonClient(const MediaTransferProtocolDaemonClient&) =
      delete;
  MediaTransferProtocolDaemonClient& operator=(
      const MediaTransferProtocolDaemonClient&) = delete;

  virtual ~MediaTransferProtocolDaemonClient();

  // Calls EnumerateStorages method. |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void EnumerateStorages(EnumerateStoragesCallback callback,
                                 ErrorCallback error_callback) = 0;

  // Calls GetStorageInfo method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  virtual void GetStorageInfo(const std::string& storage_name,
                              GetStorageInfoCallback callback,
                              ErrorCallback error_callback) = 0;

  // Calls GetStorageInfoFromDevice method. |callback| is called after the
  // method call succeeds, otherwise, |error_callback| is called.
  virtual void GetStorageInfoFromDevice(const std::string& storage_name,
                                        GetStorageInfoCallback callback,
                                        ErrorCallback error_callback) = 0;

  // Calls OpenStorage method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // OpenStorage returns a handle in |callback|.
  virtual void OpenStorage(const std::string& storage_name,
                           const std::string& mode,
                           OpenStorageCallback callback,
                           ErrorCallback error_callback) = 0;

  // Calls CloseStorage method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |handle| comes from a OpenStorageCallback.
  virtual void CloseStorage(const std::string& handle,
                            CloseStorageCallback callback,
                            ErrorCallback error_callback) = 0;

  // Calls CreateDirectory method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |parent_id| is an id of the parent directory.
  // |directory_name| is name of new directory.
  virtual void CreateDirectory(const std::string& handle,
                               const uint32_t parent_id,
                               const std::string& directory_name,
                               CreateDirectoryCallback callback,
                               ErrorCallback error_callback) = 0;

  // Calls ReadDirectoryEntryIds method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  virtual void ReadDirectoryEntryIds(const std::string& handle,
                                     uint32_t file_id,
                                     ReadDirectoryEntryIdsCallback callback,
                                     ErrorCallback error_callback) = 0;

  // Calls GetFileInfo method. |callback| is called after the method
  // call succeeds, otherwise, |error_callback| is called.
  // |file_ids| is a list of MTP-device specific file ids.
  virtual void GetFileInfo(const std::string& handle,
                           const std::vector<uint32_t>& file_ids,
                           GetFileInfoCallback callback,
                           ErrorCallback error_callback) = 0;

  // Calls ReadFileChunk method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |file_id| is a MTP-device specific id for a file.
  // |offset| is the offset into the file.
  // |bytes_to_read| cannot exceed 1 MiB.
  virtual void ReadFileChunk(const std::string& handle,
                             uint32_t file_id,
                             uint32_t offset,
                             uint32_t bytes_to_read,
                             ReadFileCallback callback,
                             ErrorCallback error_callback) = 0;

  // Calls RenameObject method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |object_is| is an id of object to be renamed.
  // |new_name| is new name of the object.
  virtual void RenameObject(const std::string& handle,
                            const uint32_t object_id,
                            const std::string& new_name,
                            RenameObjectCallback callback,
                            ErrorCallback error_callback) = 0;

  // Calls CopyFileFromLocal method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |source_file_descriptor| is a file descriptor of source file.
  // |parent_id| is a object id of a target directory.
  // |file_name| is a file name of a target file.
  virtual void CopyFileFromLocal(const std::string& handle,
                                 const int source_file_descriptor,
                                 const uint32_t parent_id,
                                 const std::string& file_name,
                                 CopyFileFromLocalCallback callback,
                                 ErrorCallback error_callback) = 0;

  // Calls DeleteObject method. |callback| is called after the method call
  // succeeds, otherwise, |error_callback| is called.
  // |object_id| is an object id of a file or directory which is deleted.
  virtual void DeleteObject(const std::string& handle,
                            const uint32_t object_id,
                            DeleteObjectCallback callback,
                            ErrorCallback error_callback) = 0;

  // Registers given callback for events. Should only be called once.
  // |storage_event_handler| is called when a mtp storage attach or detach
  // signal is received.
  virtual void ListenForChanges(const MTPStorageEventHandler& handler) = 0;

  // Factory function, creates a new instance.
  static std::unique_ptr<MediaTransferProtocolDaemonClient> Create(
      dbus::Bus* bus);

 protected:
  // Create() should be used instead.
  MediaTransferProtocolDaemonClient();
};

}  // namespace device

#endif  // SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MEDIA_TRANSFER_PROTOCOL_DAEMON_CLIENT_H_
