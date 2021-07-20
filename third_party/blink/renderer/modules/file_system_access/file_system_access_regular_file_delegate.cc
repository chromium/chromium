// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::Create(
    base::File backing_file) {
  return MakeGarbageCollected<FileSystemAccessRegularFileDelegate>(
      std::move(backing_file), base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessRegularFileDelegate::FileSystemAccessRegularFileDelegate(
    base::File backing_file,
    base::PassKey<FileSystemAccessFileDelegate>)
    : backing_file_(std::move(backing_file)) {}

FileErrorOr<int> FileSystemAccessRegularFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

FileErrorOr<int> FileSystemAccessRegularFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

FileErrorOr<int64_t> FileSystemAccessRegularFileDelegate::GetLength() {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return 0;
}

bool FileSystemAccessRegularFileDelegate::SetLength(int64_t length) {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool FileSystemAccessRegularFileDelegate::Flush() {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

void FileSystemAccessRegularFileDelegate::Close() {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
}

}  // namespace blink
