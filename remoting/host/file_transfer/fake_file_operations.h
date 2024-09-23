// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "remoting/host/file_transfer/file_operations.h"
#include "remoting/proto/file_transfer.pb.h"

namespace remoting {

// Fake FileOperations implementation for testing. Outputs written files to a
// vector.
class FakeFileOperations : public FileOperations {
 public:
  struct OutputFile {
    OutputFile(base::FilePath filename,
               bool failed,
               std::vector<std::vector<std::uint8_t>> chunks);
    OutputFile(const OutputFile& other);
    OutputFile(OutputFile&& other);
    OutputFile& operator=(const OutputFile&);
    OutputFile& operator=(OutputFile&&);
    ~OutputFile();

    // The filename provided to Open.
    base::FilePath filename;

    // True if the file was canceled or returned an error due to io_error being
    // set. False if the file was written and closed successfully.
    bool failed;

    // All of the chunks successfully written before close/cancel/error.
    std::vector<std::vector<std::uint8_t>> chunks;
  };

  struct InputFile {
    InputFile();
    InputFile(base::FilePath filename,
              std::vector<std::uint8_t> data,
              std::optional<protocol::FileTransfer_Error> io_error);
    InputFile(const InputFile& other);
    InputFile(InputFile&& other);
    InputFile& operator=(const InputFile&);
    InputFile& operator=(InputFile&&);
    ~InputFile();

    // The filename reported by the reader.
    base::FilePath filename;

    // The file data to provide in response to read requests.
    std::vector<std::uint8_t> data;

    // If set, this error will be returned instead of EOF once the provided data
    // has been read.
    std::optional<protocol::FileTransfer_Error> io_error;
  };

  // Used to interact with FakeFileOperations after ownership is passed
  // elsewhere.
  struct TestIo {
    TestIo();
    TestIo(const TestIo& other);
    ~TestIo();

    // The file information used for the next call to Reader::Open. If an error,
    // it will be returned from the Open call.
    protocol::FileTransferResult<InputFile> input_file;

    // An element will be added for each file written in full or in part.
    std::vector<OutputFile> files_written;

    // If set, file operations will return this error.
    std::optional<protocol::FileTransfer_Error> io_error = std::nullopt;
  };

  explicit FakeFileOperations(TestIo* test_io);
  ~FakeFileOperations() override;

  // FileOperations implementation.
  std::unique_ptr<Reader> CreateReader() override;
  std::unique_ptr<Writer> CreateWriter() override;

 private:
  class FakeFileReader;
  class FakeFileWriter;

  raw_ptr<TestIo> test_io_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FAKE_FILE_OPERATIONS_H_
