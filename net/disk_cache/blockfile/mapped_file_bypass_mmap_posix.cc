// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <stdlib.h>

#include "base/check.h"
#include "base/files/file_path.h"

namespace disk_cache {

void* MappedFile::Init(const base::FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return nullptr;

  if (!size)
    size = GetLength();

  buffer_ = base::HeapArray<uint8_t>::Uninit(size);
  if (Read(buffer_, 0)) {
    snapshot_ = base::HeapArray<uint8_t>::CopiedFrom(buffer_.as_span());
    view_size_ = size;
  } else {
    buffer_ = base::HeapArray<uint8_t>();
    view_size_ = 0;
  }

  init_ = true;
  return buffer_.data();
}

void MappedFile::Flush() {
  const size_t block_size = 4096;
  for (size_t offset = 0; offset < view_size_; offset += block_size) {
    size_t size = std::min(view_size_ - offset, block_size);
    base::span<const uint8_t> buffer_portion =
        buffer_.as_span().subspan(offset, size);
    base::span<uint8_t> snapshot_portion =
        snapshot_.as_span().subspan(offset, size);

    if (snapshot_portion != buffer_portion) {
      snapshot_portion.copy_from_nonoverlapping(buffer_portion);
      Write(snapshot_portion, offset);
    }
  }
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  Flush();
}

}  // namespace disk_cache
