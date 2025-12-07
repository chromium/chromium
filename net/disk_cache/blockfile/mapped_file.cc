// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "base/containers/heap_array.h"

namespace disk_cache {

// Note: Most of this class is implemented in platform-specific files.

MappedFile::MappedFile() : File(true) {}

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->as_span(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->as_span(), offset);
}

bool MappedFile::Preload() {
  size_t file_len = GetLength();
  auto buf = base::HeapArray<uint8_t>::Uninit(file_len);
  if (!Read(buf, 0)) {
    return false;
  }
  return true;
}
}  // namespace disk_cache
