// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_FILE_BLOCK_H_
#define NET_DISK_CACHE_BLOCKFILE_FILE_BLOCK_H_

#include <stddef.h>

namespace disk_cache {

// This interface exposes common functionality for a single block of data
// stored on a file-block, regardless of the real type or size of the block.
// Used to simplify loading / storing the block from disk.
class FileBlock {
 public:
  virtual ~FileBlock() = default;

  // Returns a pointer to the actual data.
  virtual void* buffer() const = 0;

  // Returns the size of the block;
  virtual size_t size() const = 0;

  // Returns the file offset of this block.
  virtual int offset() const = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_FILE_BLOCK_H_
