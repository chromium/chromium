// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "storage/browser/file_system/async_file_util.h"
#include "url/origin.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
}  // namespace storage

namespace content {
class MediaLicenseStorageHost;

// This class implements the media::mojom::CdmFile interface. It uses the same
// mojo pipe as CdmStorageImpl, to enforce message dispatch order.
class CdmFileImpl final : public media::mojom::CdmFile {
 public:
  // Check whether |name| is valid as a usable file name. Returns true if it is,
  // false otherwise.
  static bool IsValidName(const std::string& name);

  // This "file" is actually just an entry in a custom backend for CDM, uniquely
  // identified by a storage key, CDM type, and file name. File operations are
  // routed through `host` which is owned by the storage partition.
  CdmFileImpl(
      MediaLicenseStorageHost* host,
      const media::CdmType& cdm_type,
      const std::string& file_name,
      mojo::PendingAssociatedReceiver<media::mojom::CdmFile> pending_receiver);

  CdmFileImpl(const std::string& file_name,
              const url::Origin& origin,
              const media::CdmType& cdm_type,
              const std::string& file_system_root_uri,
              scoped_refptr<storage::FileSystemContext> file_system_context);

  CdmFileImpl(const CdmFileImpl&) = delete;
  CdmFileImpl& operator=(const CdmFileImpl&) = delete;

  ~CdmFileImpl() final;

  // Called to grab a lock on the file. Returns false if the file is in use by
  // other CDMs or by the system, true otherwise. Note that |this| should not
  // be used anymore if Initialize() fails.
  bool Initialize();

  // media::mojom::CdmFile implementation.
  void Read(ReadCallback callback) final;
  void Write(const std::vector<uint8_t>& data, WriteCallback callback) final;

 private:
  class FileReader;
  class FileWriter;

  // Called when the file is read. If |success| is true, |data| is the contents
  // of the file read.
  void ReadDone(bool success, std::vector<uint8_t> data);

  // Called in sequence to write the file. |buffer| is the contents to be
  // written to the file, |bytes_to_write| is the length. Uses |file_writer_|,
  // which is cleared when no longer needed. |write_callback_| will always be
  // called with the result.
  void OnEnsureTempFileExists(scoped_refptr<net::IOBuffer> buffer,
                              int bytes_to_write,
                              base::File::Error result,
                              bool created);
  void OnTempFileIsEmpty(scoped_refptr<net::IOBuffer> buffer,
                         int bytes_to_write,
                         base::File::Error result);
  void WriteDone(bool success);
  void OnFileRenamed(base::File::Error move_result);

  // Deletes |file_name_| asynchronously.
  void DeleteFile();
  void OnFileDeleted(base::File::Error result);

  // Returns the FileSystemURL for the specified |file_name|.
  storage::FileSystemURL CreateFileSystemURL(const std::string& file_name);

  // Helper methods to lock and unlock a file.
  bool AcquireFileLock(const std::string& file_name);
  void ReleaseFileLock(const std::string& file_name);

  // Report operation time to UMA.
  void ReportFileOperationTimeUMA(const std::string& uma_name);

  void ReadUsingMediaLicenseStorageDelegate();
  void DidReadUsingMediaLicenseStorageDelegate(
      absl::optional<std::vector<uint8_t>> data);
  void WriteUsingMediaLicenseStorageDelegate(const std::vector<uint8_t>& data);
  void DidWriteUsingMediaLicenseStorageDelegate(bool success);
  void DeleteUsingMediaLicenseStorageDelegate();
  void DidDeleteUsingMediaLicenseStorageDelegate(bool success);

  void OnReceiverDisconnect();

  // This receiver is associated with the CdmStorage receiver which creates it.
  mojo::AssociatedReceiver<media::mojom::CdmFile> receiver_{this};

  // Names of the files this class represents.
  const std::string file_name_;
  const std::string temp_file_name_;

  // TODO(crbug.com/1231162): Remove some of these fields and update this
  // comment when we migrate off of the PluginPrivateFileSystem.
  // Files are stored in the PluginPrivateFileSystem. The following are needed
  // to access files.
  const url::Origin origin_;
  const media::CdmType cdm_type_;
  const std::string file_system_root_uri_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Keep track of when the original file |file_name_| is locked.
  // Initialize() can only be called if false and takes the lock (on success).
  // Read() and Write() can only be called if true.
  // Note that having a lock on |file_name| implies that |temp_file_name| is
  // reserved for use by this object only, and an explicit lock on
  // |temp_file_name| is not required.
  bool file_locked_ = false;

  // Used when reading the file. |file_reader_| lives on the IO thread.
  ReadCallback read_callback_;
  base::SequenceBound<FileReader> file_reader_;

  // Used when writing the file. |file_writer_| lives on the IO thread.
  WriteCallback write_callback_;
  base::SequenceBound<FileWriter> file_writer_;

  // Time when the read or write operation starts.
  base::TimeTicks start_time_;

  // Backing store which CDM file operations are routed through.
  // Owned by MediaLicenseManager.
  const raw_ptr<MediaLicenseStorageHost> host_ = nullptr;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CdmFileImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
