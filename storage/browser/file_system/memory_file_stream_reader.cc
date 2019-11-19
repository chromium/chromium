// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_reader.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"

namespace storage {

std::unique_ptr<FileStreamReader> FileStreamReader::CreateForMemoryFile(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time) {
  return base::WrapUnique(
      new MemoryFileStreamReader(std::move(memory_file_util), file_path,
                                 initial_offset, expected_modification_time));
}

MemoryFileStreamReader::MemoryFileStreamReader(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : memory_file_util_(std::move(memory_file_util)),
      file_path_(file_path),
      expected_modification_time_(expected_modification_time),
      offset_(initial_offset) {
  DCHECK(memory_file_util_);
}

MemoryFileStreamReader::~MemoryFileStreamReader() = default;

int MemoryFileStreamReader::Read(net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback /*callback*/) {
  base::File::Info file_info;
  if (memory_file_util_->GetFileInfo(file_path_, &file_info) !=
      base::File::FILE_OK) {
    return net::ERR_FILE_NOT_FOUND;
  }

  if (!FileStreamReader::VerifySnapshotTime(expected_modification_time_,
                                            file_info)) {
    return net::ERR_UPLOAD_FILE_CHANGED;
  }

  int result = memory_file_util_->ReadFile(file_path_, offset_, buf, buf_len);
  if (result > 0)
    offset_ += result;
  return result;
}

int64_t MemoryFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback /*callback*/) {
  base::File::Info file_info;
  if (memory_file_util_->GetFileInfo(file_path_, &file_info) !=
      base::File::FILE_OK) {
    return net::ERR_FILE_NOT_FOUND;
  }

  if (!FileStreamReader::VerifySnapshotTime(expected_modification_time_,
                                            file_info)) {
    return net::ERR_UPLOAD_FILE_CHANGED;
  }

  return file_info.size;
}

}  // namespace storage
