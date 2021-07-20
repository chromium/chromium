// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_

#include "base/files/file.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_file_delegate.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// Non-incognito implementation of the FileSystemAccessFileDelegate. This class
// is a thin wrapper around an OS-level file descriptor.
class FileSystemAccessRegularFileDelegate final
    : public FileSystemAccessFileDelegate {
 public:
  // Instances should only be constructed via
  // `FileSystemAccessFileDelegate::Create()`
  explicit FileSystemAccessRegularFileDelegate(
      base::File backing_file,
      base::PassKey<FileSystemAccessFileDelegate>);

  FileSystemAccessRegularFileDelegate(
      const FileSystemAccessRegularFileDelegate&) = delete;
  FileSystemAccessRegularFileDelegate& operator=(
      const FileSystemAccessRegularFileDelegate&) = delete;

  FileErrorOr<int> Read(int64_t offset, base::span<uint8_t> data) override;
  FileErrorOr<int> Write(int64_t offset,
                         const base::span<uint8_t> data) override;

  FileErrorOr<int64_t> GetLength() override;
  bool SetLength(int64_t length) override;

  bool Flush() override;
  void Close() override;

  bool IsValid() const override { return backing_file_.IsValid(); }

 private:
  // The file on disk backing the parent FileSystemFileHandle.
  base::File backing_file_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_REGULAR_FILE_DELEGATE_H_
