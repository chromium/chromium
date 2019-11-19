// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

#include <windows.h>

namespace disk_cache {

void* MappedFile::Init(const base::FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return nullptr;

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
  std::unique_ptr<char[]> temp(new char[temp_len]);
  if (!Read(temp.get(), temp_len, 0))
    return nullptr;

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
