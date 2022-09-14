// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <algorithm>
#include <memory>

namespace disk_cache {

// Note: Most of this class is implemented in platform-specific files.

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset);
}

bool MappedFile::Preload() {
  size_t file_len = GetLength();
  auto buf = std::make_unique<char[]>(file_len);
  if (!Read(buf.get(), file_len, 0))
    return false;
  return true;
}
}  // namespace disk_cache
