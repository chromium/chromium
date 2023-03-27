// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_MAPPED_FILE_H_
#define NET_DISK_CACHE_BLOCKFILE_MAPPED_FILE_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/file.h"
#include "net/disk_cache/blockfile/file_block.h"
#include "net/net_buildflags.h"

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
  MappedFile() : File(true) {}

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  // Performs object initialization. name is the file to use, and size is the
  // amount of data to memory map from the file. If size is 0, the whole file
  // will be mapped in memory.
  void* Init(const base::FilePath& name, size_t size);

  void* buffer() const {
    return buffer_;
  }

  // Loads or stores a given block from the backing file (synchronously).
  bool Load(const FileBlock* block);
  bool Store(const FileBlock* block);

  // Flush the memory-mapped section to disk (synchronously).
  void Flush();

  // Heats up the file system cache and make sure the file is fully
  // readable (synchronously).
  bool Preload();

 private:
  ~MappedFile() override;

  bool init_ = false;
#if BUILDFLAG(IS_WIN)
  HANDLE section_;
#endif
  // This field is not a raw_ptr<> because it is using mmap, MapViewOfFile or
  // base::AllocPages directly.
  // TODO(bartekn): This one has a malloc() path, consider rewriting after all.
  RAW_PTR_EXCLUSION void* buffer_;  // Address of the memory mapped buffer.
  size_t view_size_;  // Size of the memory pointed by buffer_.
#if BUILDFLAG(POSIX_BYPASS_MMAP)
  raw_ptr<void>
      snapshot_;  // Copy of the buffer taken when it was last flushed.
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
