// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_IPC_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_IPC_FILE_OPERATIONS_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace remoting {

// Implementation of FileOperations that forwards a file read or write request
// over a Mojo channel.
class IpcFileOperations : public FileOperations {
 public:
  using BeginFileReadCallback =
      mojom::DesktopSessionControl::BeginFileReadCallback;
  using BeginFileWriteCallback =
      mojom::DesktopSessionControl::BeginFileWriteCallback;

  // Handles requests from an IpcFileOperations instance by forwarding them to
  // another process via Mojo IPC.
  class RequestHandler {
   public:
    virtual ~RequestHandler() = default;

    virtual void BeginFileRead(BeginFileReadCallback callback,
                               base::OnceClosure on_disconnect) = 0;
    virtual void BeginFileWrite(const base::FilePath& file_path,
                                BeginFileWriteCallback callback,
                                base::OnceClosure on_disconnect) = 0;
  };

  IpcFileOperations(const IpcFileOperations&) = delete;
  IpcFileOperations& operator=(const IpcFileOperations&) = delete;

  ~IpcFileOperations() override;

  // FileOperations implementation.
  std::unique_ptr<Reader> CreateReader() override;
  std::unique_ptr<Writer> CreateWriter() override;

 private:
  class IpcReader;
  class IpcWriter;

  explicit IpcFileOperations(base::WeakPtr<RequestHandler> request_handler);

  base::WeakPtr<RequestHandler> request_handler_;

  friend class IpcFileOperationsFactory;
};

// Creates IpcFileOperations instances for a given RequestHandler. All
// IpcFileOperations instances for the RequestHandler must be created through
// the same IpcFileOperationsFactory.
class IpcFileOperationsFactory {
 public:
  // |request_handler| must outlive the IpcFileOperationsFactory instance.
  explicit IpcFileOperationsFactory(
      IpcFileOperations::RequestHandler* request_handler);

  IpcFileOperationsFactory(const IpcFileOperationsFactory&) = delete;
  IpcFileOperationsFactory& operator=(const IpcFileOperationsFactory&) = delete;

  ~IpcFileOperationsFactory();

  std::unique_ptr<FileOperations> CreateFileOperations();

 private:
  base::WeakPtrFactory<IpcFileOperations::RequestHandler>
      request_handler_weak_ptr_factory_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_IPC_FILE_OPERATIONS_H_
