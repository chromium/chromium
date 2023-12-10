// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For a general description of the files used by the cache see file_format.h.
//
// A block file is a file designed to store blocks of data of a given size. It
// is able to store data that spans from one to four consecutive "blocks", and
// it grows as needed to store up to approximately 65000 blocks. It has a fixed
// size header used for book keeping such as tracking free of blocks on the
// file. For example, a block-file for 1KB blocks will grow from 8KB when
// totally empty to about 64MB when completely full. At that point, data blocks
// of 1KB will be stored on a second block file that will store the next set of
// 65000 blocks. The first file contains the number of the second file, and the
// second file contains the number of a third file, created when the second file
// reaches its limit. It is important to remember that no matter how long the
// chain of files is, any given block can be located directly by its address,
// which contains the file number and starting block inside the file.

#ifndef NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_BASE_H_
#define NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_BASE_H_

#include <stdint.h>

namespace disk_cache {

typedef uint32_t CacheAddr;

const uint32_t kBlockVersion2 = 0x20000;        // Version 2.0.

const uint32_t kBlockMagic = 0xC104CAC3;
const int kBlockHeaderSize = 8192;  // Two pages: almost 64k entries
const int kMaxBlocks = (kBlockHeaderSize - 80) * 8;
const int kNumExtraBlocks = 1024;  // How fast files grow.

// Bitmap to track used blocks on a block-file.
typedef uint32_t AllocBitmap[kMaxBlocks / 32];

// A block-file is the file used to store information in blocks (could be
// EntryStore blocks, RankingsNode blocks or user-data blocks).
// We store entries that can expand for up to 4 consecutive blocks, and keep
// counters of the number of blocks available for each type of entry. For
// instance, an entry of 3 blocks is an entry of type 3. We also keep track of
// where did we find the last entry of that type (to avoid searching the bitmap
// from the beginning every time).
// This Structure is the header of a block-file:
struct BlockFileHeader {
  uint32_t magic;
  uint32_t version;
  int16_t this_file;          // Index of this file.
  int16_t next_file;          // Next file when this one is full.
  int32_t entry_size;         // Size of the blocks of this file.
  int32_t num_entries;        // Number of stored entries.
  int32_t max_entries;        // Current maximum number of entries.
  int32_t empty[4];           // Counters of empty entries for each type.
  int32_t hints[4];           // Last used position for each entry type.
  volatile int32_t updating;  // Keep track of updates to the header.
  int32_t user[5];
  AllocBitmap     allocation_map;
};

static_assert(sizeof(BlockFileHeader) == kBlockHeaderSize, "bad header");

// Sparse data support:
// We keep a two level hierarchy to enable sparse data for an entry: the first
// level consists of using separate "child" entries to store ranges of 1 MB,
// and the second level stores blocks of 1 KB inside each child entry.
//
// Whenever we need to access a particular sparse offset, we first locate the
// child entry that stores that offset, so we discard the 20 least significant
// bits of the offset, and end up with the child id. For instance, the child id
// to store the first megabyte is 0, and the child that should store offset
// 0x410000 has an id of 4.
//
// The child entry is stored the same way as any other entry, so it also has a
// name (key). The key includes a signature to be able to identify children
// created for different generations of the same resource. In other words, given
// that a given sparse entry can have a large number of child entries, and the
// resource can be invalidated and replaced with a new version at any time, it
// is important to be sure that a given child actually belongs to certain entry.
//
// The full name of a child entry is composed with a prefix ("Range_"), and two
// hexadecimal 64-bit numbers at the end, separated by semicolons. The first
// number is the signature of the parent key, and the second number is the child
// id as described previously. The signature itself is also stored internally by
// the child and the parent entries. For example, a sparse entry with a key of
// "sparse entry name", and a signature of 0x052AF76, may have a child entry
// named "Range_sparse entry name:052af76:4", which stores data in the range
// 0x400000 to 0x4FFFFF.
//
// Each child entry keeps track of all the 1 KB blocks that have been written
// to the entry, but being a regular entry, it will happily return zeros for any
// read that spans data not written before. The actual sparse data is stored in
// one of the data streams of the child entry (at index 1), while the control
// information is stored in another stream (at index 2), both by parents and
// the children.

// This structure contains the control information for parent and child entries.
// It is stored at offset 0 of the data stream with index 2.
// It is possible to write to a child entry in a way that causes the last block
// to be only partialy filled. In that case, last_block and last_block_len will
// keep track of that block.
struct SparseHeader {
  int64_t signature;       // The parent and children signature.
  uint32_t magic;          // Structure identifier (equal to kIndexMagic).
  int32_t parent_key_len;  // Key length for the parent entry.
  int32_t last_block;      // Index of the last written block.
  int32_t last_block_len;  // Length of the last written block.
  int32_t dummy[10];
};

// The SparseHeader will be followed by a bitmap, as described by this
// structure.
struct SparseData {
  SparseHeader header;
  uint32_t bitmap[32];  // Bitmap representation of known children (if this
                        // is a parent entry), or used blocks (for child
                        // entries. The size is fixed for child entries but
                        // not for parents; it can be as small as 4 bytes
                        // and as large as 8 KB.
};

// The number of blocks stored by a child entry.
const int kNumSparseBits = 1024;
static_assert(sizeof(SparseData) == sizeof(SparseHeader) + kNumSparseBits / 8,
              "invalid SparseData bitmap");

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_BASE_H_
