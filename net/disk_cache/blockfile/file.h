// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_FILE_H_
#define NET_DISK_CACHE_BLOCKFILE_FILE_H_

#include <stddef.h>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
}

namespace disk_cache {

// This interface is used to support asynchronous ReadData and WriteData calls.
class FileIOCallback {
 public:
  // Notified of the actual number of bytes read or written. This value is
  // negative if an error occurred.
  virtual void OnFileIOComplete(int bytes_copied) = 0;

 protected:
  virtual ~FileIOCallback() = default;
};

// Simple wrapper around a file that allows asynchronous operations.
class NET_EXPORT_PRIVATE File : public base::RefCounted<File> {
  friend class base::RefCounted<File>;
 public:
  File();
  // mixed_mode set to true enables regular synchronous operations for the file.
  explicit File(bool mixed_mode);

  // Initializes the object to use the passed in file instead of opening it with
  // the Init() call. No asynchronous operations can be performed with this
  // object.
  explicit File(base::File file);

  File(const File&) = delete;
  File& operator=(const File&) = delete;

  // Initializes the object to point to a given file. The file must aready exist
  // on disk, and allow shared read and write.
  bool Init(const base::FilePath& name);

  // Returns true if the file was opened properly.
  bool IsValid() const;

  // Performs synchronous IO.
  bool Read(void* buffer, size_t buffer_len, size_t offset);
  bool Write(const void* buffer, size_t buffer_len, size_t offset);

  // Performs asynchronous IO. callback will be called when the IO completes,
  // as an APC on the thread that queued the operation.
  bool Read(void* buffer, size_t buffer_len, size_t offset,
            FileIOCallback* callback, bool* completed);
  bool Write(const void* buffer, size_t buffer_len, size_t offset,
             FileIOCallback* callback, bool* completed);

  // Sets the file's length. The file is truncated or extended with zeros to
  // the new length.
  bool SetLength(size_t length);
  size_t GetLength();

  // Blocks until |num_pending_io| IO operations complete.
  static void WaitForPendingIOForTesting(int* num_pending_io);

  // Drops current pending operations without waiting for them to complete.
  static void DropPendingIO();

 protected:
  virtual ~File();

  // Returns the handle or file descriptor.
  base::PlatformFile platform_file() const;

 private:
  // Performs the actual asynchronous write. If notify is set and there is no
  // callback, the call will be re-synchronized.
  bool AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                  FileIOCallback* callback, bool* completed);

  // Infrastructure for async IO.
  int DoRead(void* buffer, size_t buffer_len, size_t offset);
  int DoWrite(const void* buffer, size_t buffer_len, size_t offset);
  void OnOperationComplete(FileIOCallback* callback, int result);

  bool init_;
  bool mixed_;
  base::File base_file_;  // Regular, asynchronous IO handle.
  base::File sync_base_file_;  // Synchronous IO handle.
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_FILE_H_
