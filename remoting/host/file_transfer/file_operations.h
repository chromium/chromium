// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_
#define REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace base {
class FilePath;
}

namespace remoting {

// Interface for reading and writing file transfers.

class FileOperations {
 public:
  enum State {
    // The Reader/Writer has been newly created. No file is open, yet.
    kCreated = 0,

    // The file has been opened. WriteChunk(), ReadChunk(), and Close() can be
    // called.
    kReady = 1,

    // A file operation is currently being processed. WriteChunk(), ReadChunk(),
    // and Close() cannot be called until the state changes back to kReady.
    kBusy = 2,

    // EOF has been reached (when reading) or Close() has been called and
    // succeeded (when writing).
    kComplete = 3,

    // An error has occurred.
    kFailed = 4,
  };

  class Reader {
   public:
    using OpenResult = protocol::FileTransferResult<absl::monostate>;
    using OpenCallback = base::OnceCallback<void(OpenResult result)>;

    // On success, |result| will contain the read data, or an empty vector on
    // EOF. Once EOF is reached, the state will transition to kComplete and no
    // more operations may be performed. The reader will attempt to read as much
    // data as requested, but there may be cases where less data is returned.
    using ReadResult = protocol::FileTransferResult<std::vector<std::uint8_t>>;
    using ReadCallback = base::OnceCallback<void(ReadResult result)>;

    // Once destroyed, no further callbacks will be invoked.
    virtual ~Reader() = default;

    // Prompt the user to select a file and open it for reading.
    virtual void Open(OpenCallback callback) = 0;

    // Reads a chunk of the given size from the file.
    virtual void ReadChunk(std::size_t size, ReadCallback callback) = 0;

    virtual const base::FilePath& filename() const = 0;
    virtual std::uint64_t size() const = 0;
    virtual State state() const = 0;
  };

  class Writer {
   public:
    using Result = protocol::FileTransferResult<absl::monostate>;
    using Callback = base::OnceCallback<void(Result result)>;

    // Destructing before the file is completely written and closed will
    // automatically delete the partial file. If the Writer is destroyed after
    // calling Close but before the associated callback is invoked, the file may
    // either be complete or deleted, depending on the exact timing. In any
    // event, no further callbacks will be invoked once the object is destroyed.
    virtual ~Writer() = default;

    // Starts writing a new file to the default location. This will create a
    // temp file at the location, which will be renamed when writing is
    // complete.
    virtual void Open(const base::FilePath& filename, Callback callback) = 0;

    // Writes a chunk to the file. Chunks cannot be queued; the caller must
    // wait until callback is called before calling WriteChunk again or calling
    // Close.
    virtual void WriteChunk(std::vector<std::uint8_t> data,
                            Callback callback) = 0;

    // Closes the file, flushing any data still in the OS buffer and moving the
    // the file to its final location.
    virtual void Close(Callback callback) = 0;

    virtual State state() const = 0;
  };

  FileOperations() = default;

  FileOperations(const FileOperations&) = delete;
  FileOperations& operator=(const FileOperations&) = delete;

  virtual ~FileOperations() = default;

  virtual std::unique_ptr<Reader> CreateReader() = 0;
  virtual std::unique_ptr<Writer> CreateWriter() = 0;
};
}  // namespace remoting

#endif  // REMOTING_HOST_FILE_TRANSFER_FILE_OPERATIONS_H_
