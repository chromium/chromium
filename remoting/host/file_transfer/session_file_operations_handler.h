// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_

#include <memory>

#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace remoting {

// A Mojo-aware FileOperations wrapper which handles requests to begin file read
// and write operations. This class can handle concurrent requests as it creates
// a new handler for each one and ensures and resource are released when the
// new Mojo channel is closed.
class SessionFileOperationsHandler {
 public:
  explicit SessionFileOperationsHandler(
      std::unique_ptr<FileOperations> file_operations);

  SessionFileOperationsHandler(const SessionFileOperationsHandler&) = delete;
  SessionFileOperationsHandler& operator=(const SessionFileOperationsHandler&) =
      delete;

  ~SessionFileOperationsHandler();

  using BeginFileReadCallback =
      base::OnceCallback<void(mojom::BeginFileReadResultPtr)>;
  void BeginFileRead(BeginFileReadCallback callback);

  using BeginFileWriteCallback =
      base::OnceCallback<void(mojom::BeginFileWriteResultPtr)>;
  void BeginFileWrite(const base::FilePath& file_path,
                      BeginFileWriteCallback callback);

 private:
  friend class IpcFileOperationsTest;

  const std::unique_ptr<FileOperations> file_operations_;

  mojo::UniqueAssociatedReceiverSet<mojom::FileReader> file_readers_;
  mojo::UniqueAssociatedReceiverSet<mojom::FileWriter> file_writers_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
