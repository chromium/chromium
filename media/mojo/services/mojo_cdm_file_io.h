// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Implements a cdm::FileIO that communicates with mojom::CdmStorage.
class MEDIA_MOJO_EXPORT MojoCdmFileIO : public cdm::FileIO {
 public:
  class Delegate {
   public:
    // Notifies the delegate to close |cdm_file_io|.
    virtual void CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) = 0;

    // Reports the size of file read by MojoCdmFileIO.
    virtual void ReportFileReadSize(int file_size_bytes) = 0;
  };

  // The constructor and destructor of cdm::FileIO are protected so that the CDM
  // cannot delete the object directly. Here we declare the constructor and
  // destructor as public so that we can use std::unique_ptr<> for better memory
  // management.
  MojoCdmFileIO(Delegate* delegate,
                cdm::FileIOClient* client,
                mojo::Remote<mojom::CdmStorage> cdm_storage);
  MojoCdmFileIO(const MojoCdmFileIO&) = delete;
  MojoCdmFileIO operator=(const MojoCdmFileIO&) = delete;
  ~MojoCdmFileIO() override;

  // cdm::FileIO implementation.
  void Open(const char* file_name, uint32_t file_name_size) final;
  void Read() final;
  void Write(const uint8_t* data, uint32_t data_size) final;
  void Close() final;

 private:
  // Allowed state transitions:
  //   kUnopened -> kOpening -> kOpened
  //   kUnopened -> kOpening -> kUnopened (if file in use)
  //   kUnopened -> kOpening -> kError (if file not available)
  //   kOpened -> kReading -> kOpened
  //   kOpened -> kWriting -> kOpened
  // Once state = kError, only Close() can be called.
  enum class State { kUnopened, kOpening, kOpened, kReading, kWriting, kError };

  // Error that needs to be reported back to the client.
  enum class ErrorType {
    kOpenError,
    kOpenInUse,
    kReadError,
    kReadInUse,
    kWriteError,
    kWriteInUse
  };

  // Called when the file is opened (or not).
  void OnFileOpened(mojom::CdmStorage::Status status,
                    mojo::PendingAssociatedRemote<mojom::CdmFile> cdm_file);

  // Called when the read operation is done.
  void OnFileRead(mojom::CdmFile::Status status,
                  const std::vector<uint8_t>& data);

  // Called when the write operation is done.
  void OnFileWritten(mojom::CdmFile::Status status);

  // Called when an error occurs. Calls client_->OnXxxxComplete with kError
  // or kInUse asynchronously. In some cases we could actually call them
  // synchronously, but since these errors shouldn't happen in normal cases,
  // we are not optimizing such cases.
  void OnError(ErrorType error);

  // Callback to notify client of error asynchronously.
  void NotifyClientOfError(ErrorType error);

  raw_ptr<Delegate> delegate_ = nullptr;

  // Results of cdm::FileIO operations are sent asynchronously via |client_|.
  raw_ptr<cdm::FileIOClient, DanglingUntriaged> client_ = nullptr;

  mojo::Remote<mojom::CdmStorage> cdm_storage_;

  // Keep track of the file being used. As this class can only be used for
  // accessing a single file, once |file_name_| is set it shouldn't be changed.
  // |file_name_| is only saved for logging purposes.
  std::string file_name_;

  // |cdm_file_| is used to read and write the file and is released when the
  // file is closed so that CdmStorage can tell that the file is no longer being
  // used.
  mojo::AssociatedRemote<mojom::CdmFile> cdm_file_;

  // Keep track of operations in progress.
  State state_ = State::kUnopened;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmFileIO> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_FILE_IO_H_
