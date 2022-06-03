// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
#define REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"

namespace remoting {

// An implementation of IpcFileOperations::RequestHandler that forwards all
// operations to another FileOperations implementation.
class SessionFileOperationsHandler : public IpcFileOperations::RequestHandler {
 public:
  // |result_handler| must be valid for the entire lifetime of
  // SessionFileOperationsHandler.
  explicit SessionFileOperationsHandler(
      IpcFileOperations::ResultHandler* result_handler,
      std::unique_ptr<FileOperations> file_operations);
  ~SessionFileOperationsHandler() override;

  // IpcFileOperations::RequestHandler implementation
  void ReadFile(std::uint64_t file_id) override;
  void ReadChunk(std::uint64_t file_id, std::uint64_t size) override;
  void WriteFile(std::uint64_t file_id,
                 const base::FilePath& filename) override;
  void WriteChunk(std::uint64_t file_id,
                  std::vector<std::uint8_t> data) override;
  void Close(std::uint64_t file_id) override;
  void Cancel(std::uint64_t file_id) override;

 private:
  void OnReaderOpenResult(std::uint64_t file_id,
                          FileOperations::Reader::OpenResult result);
  void OnReaderReadResult(std::uint64_t file_id,
                          FileOperations::Reader::ReadResult result);
  void OnWriterOperationResult(std::uint64_t file_id,
                               FileOperations::Writer::Result result);
  void OnWriterCloseResult(std::uint64_t file_id,
                           FileOperations::Writer::Result result);

  IpcFileOperations::ResultHandler* result_handler_;
  std::unique_ptr<FileOperations> file_operations_;
  base::flat_map<std::uint64_t, std::unique_ptr<FileOperations::Writer>>
      writers_;
  base::flat_map<std::uint64_t, std::unique_ptr<FileOperations::Reader>>
      readers_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_SESSION_FILE_OPERATIONS_HANDLER_H_
