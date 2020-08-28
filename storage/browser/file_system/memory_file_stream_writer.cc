// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_writer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"

namespace storage {

std::unique_ptr<FileStreamWriter> FileStreamWriter::CreateForMemoryFile(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset) {
  return base::WrapUnique(new MemoryFileStreamWriter(
      std::move(memory_file_util), file_path, initial_offset));
}

MemoryFileStreamWriter::MemoryFileStreamWriter(
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset)
    : memory_file_util_(std::move(memory_file_util)),
      file_path_(file_path),
      offset_(initial_offset) {
  DCHECK(memory_file_util_);
}

MemoryFileStreamWriter::~MemoryFileStreamWriter() = default;

int MemoryFileStreamWriter::Write(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback /*callback*/) {
  base::File::Info file_info;
  if (memory_file_util_->GetFileInfo(file_path_, &file_info) !=
      base::File::FILE_OK) {
    return net::ERR_FILE_NOT_FOUND;
  }

  int result = memory_file_util_->WriteFile(file_path_, offset_, buf, buf_len);
  if (result > 0)
    offset_ += result;
  return result;
}

int MemoryFileStreamWriter::Cancel(net::CompletionOnceCallback /*callback*/) {
  return net::ERR_UNEXPECTED;
}

int MemoryFileStreamWriter::Flush(net::CompletionOnceCallback /*callback*/) {
  return net::OK;
}
}  // namespace storage
