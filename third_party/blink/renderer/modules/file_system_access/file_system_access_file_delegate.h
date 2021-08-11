// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_H_

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// File object providing a common interface for file operations for an
// AccessHandle. These methods closely map to base::File equivalents. The
// implementation of the FileDelegate will depend on whether the renderer is in
// incognito. All methods are synchronous.
// A FileDelegate may get accessed by multiple threads, even though the caller
// has to make sure that there is no concurrent access. This means that at any
// point in time there is exactly one thread that may access the FileDelegate.
class FileSystemAccessFileDelegate
    : public GarbageCollected<FileSystemAccessFileDelegate> {
 public:
  virtual ~FileSystemAccessFileDelegate() = default;

  static FileSystemAccessFileDelegate* Create(ExecutionContext* context,
                                              base::File backing_file);
  static FileSystemAccessFileDelegate* CreateForIncognito(
      ExecutionContext* context,
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileDelegateHost>
          incognito_file_remote);

  // Reads the given number of bytes (or until EOF is reached) into the span
  // starting with the given offset. Returns the number of bytes read, or a file
  // error on failure.
  virtual base::FileErrorOr<int> Read(int64_t offset,
                                      base::span<uint8_t> data) = 0;

  // Writes the span into the file at the given offset, overwriting any data
  // that was previously there. Returns the number of bytes written, or a file
  // error on failure.
  virtual base::FileErrorOr<int> Write(int64_t offset,
                                       const base::span<uint8_t> data) = 0;

  // Asynchronously get the size of the file. Returns the current size of this
  // file, or a file error on failure.
  virtual void GetLength(
      base::OnceCallback<void(base::FileErrorOr<int64_t>)> callback) = 0;

  // Asynchronously truncates the file to the given length. If |length| is
  // greater than the current size of the file, the file is extended with zeros.
  // If the file doesn't exist, |false| is returned.
  virtual void SetLength(int64_t length,
                         base::OnceCallback<void(bool)> callback) = 0;

  // Asynchronously instructs the filesystem to flush the file to disk.
  virtual void Flush(base::OnceCallback<void(bool)> callback) = 0;

  // Asynchronously close the file. Destroying this object will close the file
  // automatically.
  virtual void Close(base::OnceClosure callback) = 0;

  // Returns |true| if the file handle wrapped by this object is valid.
  virtual bool IsValid() const = 0;

  // GarbageCollected
  virtual void Trace(Visitor* visitor) const {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_FILE_DELEGATE_H_
