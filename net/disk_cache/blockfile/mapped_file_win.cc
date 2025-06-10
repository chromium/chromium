// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <windows.h>

#include <memory>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/files/file_path.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

void* MappedFile::Init(const base::FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return nullptr;

  if (!size) {
    size = GetLength();
  }

  buffer_ = nullptr;
  init_ = true;
  section_ = CreateFileMapping(platform_file(), nullptr, PAGE_READWRITE, 0,
                               static_cast<DWORD>(size), nullptr);
  if (!section_)
    return nullptr;

  buffer_ = MapViewOfFile(section_, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size);
  DCHECK(buffer_);
  view_size_ = size;

  // Make sure we detect hardware failures reading the headers.
  size_t temp_len = size ? size : 4096;
  auto temp = base::HeapArray<uint8_t>::Uninit(temp_len);
  if (!Read(temp.as_span(), 0)) {
    return nullptr;
  }

  return buffer_;
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_) {
    BOOL ret = UnmapViewOfFile(buffer_);
    DCHECK(ret);
  }

  if (section_)
    CloseHandle(section_);
}

void MappedFile::Flush() {
}

}  // namespace disk_cache
