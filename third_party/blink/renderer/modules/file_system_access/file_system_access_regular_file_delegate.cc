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
  int size = base::checked_cast<int>(data.size());
  int result =
      backing_file_.Read(offset, reinterpret_cast<char*>(data.data()), size);
  if (result >= 0) {
    return result;
  }
  return base::File::GetLastFileError();
}

FileErrorOr<int> FileSystemAccessRegularFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  int size = base::checked_cast<int>(data.size());
  int result =
      backing_file_.Write(offset, reinterpret_cast<char*>(data.data()), size);
  if (size == result) {
    return result;
  }
  return base::File::GetLastFileError();
}

FileErrorOr<int64_t> FileSystemAccessRegularFileDelegate::GetLength() {
  int64_t result = backing_file_.GetLength();
  if (result >= 0) {
    return result;
  }
  return base::File::GetLastFileError();
}

bool FileSystemAccessRegularFileDelegate::SetLength(int64_t length) {
  // TODO(crbug.com/1218431): Implement this method.
  NOTIMPLEMENTED();
  return false;
}

bool FileSystemAccessRegularFileDelegate::Flush() {
  return backing_file_.Flush();
}

void FileSystemAccessRegularFileDelegate::Close() {
  backing_file_.Close();
}

}  // namespace blink
