// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_MAPPED_FILE_H_
#define NET_DISK_CACHE_BLOCKFILE_MAPPED_FILE_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/blockfile/file_block.h"
#include "net/net_buildflags.h"

#if BUILDFLAG(POSIX_BYPASS_MMAP)
#include "base/containers/heap_array.h"
#endif

namespace base {
class FilePath;
}

namespace disk_cache {

// This class implements a memory mapped file used to access block-files. The
// idea is that the header and bitmap will be memory mapped all the time, and
// the actual data for the blocks will be access asynchronously (most of the
// time).
class NET_EXPORT_PRIVATE MappedFile : public File {
 public:
  MappedFile();

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  // Performs object initialization. name is the file to use, and size is the
  // amount of data to memory map from the file. If size is 0, the whole file
  // will be mapped in memory.
  void* Init(const base::FilePath& name, size_t size);

#if BUILDFLAG(POSIX_BYPASS_MMAP)
  void* buffer() { return reinterpret_cast<void*>(buffer_.data()); }

  base::span<uint8_t> as_span() { return buffer_.as_span(); }
#else
  void* buffer() { return buffer_; }

  base::span<uint8_t> as_span() {
    // SAFETY: Class invariant is that a view of `buffer_` of size `view_size_`
    // is mapped.
    return UNSAFE_BUFFERS(
        base::span(reinterpret_cast<uint8_t*>(buffer_), view_size_));
  }
#endif

  // Loads or stores a given block from the backing file (synchronously).
  bool Load(const FileBlock* block);
  bool Store(const FileBlock* block);

  // Flush the memory-mapped section to disk (synchronously).
  void Flush();
#if BUILDFLAG(IS_WIN)
  void EnableFlush();
#endif

  // Heats up the file system cache and make sure the file is fully
  // readable (synchronously).
  bool Preload();

 private:
  ~MappedFile() override;

  bool init_ = false;

#if BUILDFLAG(IS_WIN)
  bool enable_flush_ = false;
  HANDLE section_;
#endif

  size_t view_size_ = 0;  // Size of the memory pointed by `buffer_`.

#if BUILDFLAG(POSIX_BYPASS_MMAP)
  // Copy of the buffer taken when it was last flushed.
  base::HeapArray<uint8_t> snapshot_;

  // Current buffer contents.
  base::HeapArray<uint8_t> buffer_;
#else
  // Address of the memory mapped buffer.
  // This field is not a raw_ptr<> because it is using mmap or MapViewOfFile
  // directly.
  RAW_PTR_EXCLUSION void* buffer_ = nullptr;
#endif
};

// Helper class for calling Flush() on exit from the current scope.
class ScopedFlush {
 public:
  explicit ScopedFlush(MappedFile* file) : file_(file) {}
  ~ScopedFlush() {
    file_->Flush();
  }
 private:
  raw_ptr<MappedFile> file_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_MAPPED_FILE_H_
