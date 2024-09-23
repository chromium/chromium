// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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

  buffer_ = malloc(size);
  snapshot_ = malloc(size);
  if (buffer_ && snapshot_ && Read(buffer_, size, 0)) {
    memcpy(snapshot_, buffer_, size);
  } else {
    free(buffer_);
    free(snapshot_);
    buffer_ = nullptr;
    snapshot_ = nullptr;
  }

  init_ = true;
  view_size_ = size;
  return buffer_;
}

void MappedFile::Flush() {
  DCHECK(buffer_);
  DCHECK(snapshot_);
  const char* buffer_ptr = static_cast<const char*>(buffer_);
  char* snapshot_ptr = static_cast<char*>(snapshot_);
  const size_t block_size = 4096;
  for (size_t offset = 0; offset < view_size_; offset += block_size) {
    size_t size = std::min(view_size_ - offset, block_size);
    if (memcmp(snapshot_ptr + offset, buffer_ptr + offset, size)) {
      memcpy(snapshot_ptr + offset, buffer_ptr + offset, size);
      Write(snapshot_ptr + offset, size, offset);
    }
  }
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_ && snapshot_) {
    Flush();
  }
  free(buffer_);
  free(snapshot_);
}

}  // namespace disk_cache
