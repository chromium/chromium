// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/memory_allocator.h"

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"

#if defined(OS_WIN)

#include <windows.h>

namespace {

// The file is created in the %TEMP% folder.
// NOTE: Since the file will be used as backing for a memory allocation,
// it will never be so big that size_t cannot represent its size.
base::File CreateTempFile() {
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path))
    return base::File();

  int flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_DELETE_ON_CLOSE |
              base::File::FLAG_TEMPORARY;
  return base::File(path, flags);
}

}  // namespace

namespace courgette {

// FileMapping

FileMapping::FileMapping() : mapping_(nullptr), view_(nullptr) {}

FileMapping::~FileMapping() {
  Close();
}

bool FileMapping::InitializeView(size_t size) {
  DCHECK(view_ == nullptr);
  DCHECK(mapping_ != nullptr);
  view_ = ::MapViewOfFile(mapping_, FILE_MAP_WRITE, 0, 0, size);
  if (!view_) {
    Close();
    return false;
  }
  return true;
}

bool FileMapping::Create(HANDLE file, size_t size) {
  DCHECK(file != INVALID_HANDLE_VALUE);
  DCHECK(!valid());
  mapping_ = ::CreateFileMapping(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
  if (!mapping_)
    return false;

  return InitializeView(size);
}

void FileMapping::Close() {
  if (view_)
    ::UnmapViewOfFile(view_);
  if (mapping_)
    ::CloseHandle(mapping_);
  mapping_ = nullptr;
  view_ = nullptr;
}

bool FileMapping::valid() const {
  return view_ != nullptr;
}

void* FileMapping::view() const {
  return view_;
}

// TempMapping

TempMapping::TempMapping() {
}

TempMapping::~TempMapping() {
}

bool TempMapping::Initialize(size_t size) {
  file_ = CreateTempFile();
  if (!file_.IsValid())
    return false;

  // TODO(tommi): The assumption here is that the alignment of pointers (this)
  // is as strict or stricter than the alignment of the element type.  This is
  // not always true, e.g. __m128 has 16-byte alignment.
  size += sizeof(this);
  if (!file_.SetLength(size) ||
      !mapping_.Create(file_.GetPlatformFile(), size)) {
    file_.Close();
    return false;
  }

  TempMapping** write = reinterpret_cast<TempMapping**>(mapping_.view());
  write[0] = this;

  return true;
}

void* TempMapping::memory() const {
  uint8_t* mem = reinterpret_cast<uint8_t*>(mapping_.view());
  // The 'this' pointer is written at the start of mapping_.view(), so
  // go past it. (See Initialize()).
  if (mem)
    mem += sizeof(this);
  DCHECK(mem);
  return mem;
}

bool TempMapping::valid() const {
  return mapping_.valid();
}

// static
TempMapping* TempMapping::GetMappingFromPtr(void* mem) {
  TempMapping* ret = nullptr;
  if (mem) {
    ret = reinterpret_cast<TempMapping**>(mem)[-1];
  }
  DCHECK(ret);
  return ret;
}

}  // namespace courgette

#endif  // OS_WIN
