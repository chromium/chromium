// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCKFILE_BLOCK_FILES_H_
#define NET_DISK_CACHE_BLOCKFILE_BLOCK_FILES_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/disk_format_base.h"
#include "net/disk_cache/blockfile/mapped_file.h"

namespace base {
class ThreadChecker;
}

namespace disk_cache {

// An instance of this class represents the header of a block file in memory.
// Note that this class doesn't perform any file operation (as in it only deals
// with entities in memory).
// The header of a block file (and hence, this object) is all that is needed to
// perform common operations like allocating or releasing space for storage;
// actual access to that storage, however, is not performed through this class.
class NET_EXPORT_PRIVATE BlockHeader {
 public:
  BlockHeader();
  explicit BlockHeader(BlockFileHeader* header);
  explicit BlockHeader(MappedFile* file);
  BlockHeader(const BlockHeader& other);
  ~BlockHeader();

  // Creates a new entry of |size| blocks on the allocation map, updating the
  // apropriate counters.
  bool CreateMapBlock(int size, int* index);

  // Deletes the block pointed by |index|.
  void DeleteMapBlock(int index, int block_size);

  // Returns true if the specified block is used.
  bool UsedMapBlock(int index, int size);

  // Restores the "empty counters" and allocation hints.
  void FixAllocationCounters();

  // Returns true if the current block file should not be used as-is to store
  // more records. |block_count| is the number of blocks to allocate.
  bool NeedToGrowBlockFile(int block_count) const;

  // Returns true if this block file can be used to store an extra record of
  // size |block_count|.
  bool CanAllocate(int block_count) const;

  // Returns the number of empty blocks for this file.
  int EmptyBlocks() const;

  // Returns the minumum number of allocations that can be satisfied.
  int MinimumAllocations() const;

  // Returns the number of blocks that this file can store.
  int Capacity() const;

  // Returns true if the counters look OK.
  bool ValidateCounters() const;

  // Returns the identifiers of this and the next file (0 if there is none).
  int FileId() const;
  int NextFileId() const;

  // Returns the size of the wrapped structure (BlockFileHeader).
  int Size() const;

  // Returns a pointer to the underlying BlockFileHeader.
  BlockFileHeader* Header();

 private:
  BlockFileHeader* header_;
};

typedef std::vector<BlockHeader> BlockFilesBitmaps;

// This class handles the set of block-files open by the disk cache.
class NET_EXPORT_PRIVATE BlockFiles {
 public:
  explicit BlockFiles(const base::FilePath& path);
  ~BlockFiles();

  // Performs the object initialization. create_files indicates if the backing
  // files should be created or just open.
  bool Init(bool create_files);

  // Returns the file that stores a given address.
  MappedFile* GetFile(Addr address);

  // Creates a new entry on a block file. block_type indicates the size of block
  // to be used (as defined on cache_addr.h), block_count is the number of
  // blocks to allocate, and block_address is the address of the new entry.
  bool CreateBlock(FileType block_type, int block_count, Addr* block_address);

  // Removes an entry from the block files. If deep is true, the storage is zero
  // filled; otherwise the entry is removed but the data is not altered (must be
  // already zeroed).
  void DeleteBlock(Addr address, bool deep);

  // Close all the files and set the internal state to be initializad again. The
  // cache is being purged.
  void CloseFiles();

  // Sends UMA stats.
  void ReportStats();

  // Returns true if the blocks pointed by a given address are currently used.
  // This method is only intended for debugging.
  bool IsValid(Addr address);

 private:
  // Set force to true to overwrite the file if it exists.
  bool CreateBlockFile(int index, FileType file_type, bool force);
  bool OpenBlockFile(int index);

  // Attemp to grow this file. Fails if the file cannot be extended anymore.
  bool GrowBlockFile(MappedFile* file, BlockFileHeader* header);

  // Returns the appropriate file to use for a new block.
  MappedFile* FileForNewBlock(FileType block_type, int block_count);

  // Returns the next block file on this chain, creating new files if needed.
  MappedFile* NextFile(MappedFile* file);

  // Creates an empty block file and returns its index.
  int16_t CreateNextBlockFile(FileType block_type);

  // Removes a chained block file that is now empty.
  bool RemoveEmptyFile(FileType block_type);

  // Restores the header of a potentially inconsistent file.
  bool FixBlockFileHeader(MappedFile* file);

  // Retrieves stats for the given file index.
  void GetFileStats(int index, int* used_count, int* load);

  // Returns the filename for a given file index.
  base::FilePath Name(int index);

  bool init_;
  char* zero_buffer_;  // Buffer to speed-up cleaning deleted entries.
  base::FilePath path_;  // Path to the backing folder.
  std::vector<scoped_refptr<MappedFile>> block_files_;  // The actual files.
  std::unique_ptr<base::ThreadChecker> thread_checker_;

  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_ZeroSizeFile);
  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_TruncatedFile);
  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_InvalidFile);
  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_Stats);

  DISALLOW_COPY_AND_ASSIGN(BlockFiles);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BLOCK_FILES_H_
