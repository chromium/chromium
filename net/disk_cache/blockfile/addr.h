// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an internal class that handles the address of a cache record.
// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BLOCKFILE_ADDR_H_
#define NET_DISK_CACHE_BLOCKFILE_ADDR_H_

#include <stddef.h>
#include <stdint.h>

#include "base/notreached.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/disk_format_base.h"

namespace disk_cache {

enum FileType {
  EXTERNAL = 0,
  RANKINGS = 1,
  BLOCK_256 = 2,
  BLOCK_1K = 3,
  BLOCK_4K = 4,
  BLOCK_FILES = 5,
  BLOCK_ENTRIES = 6,
  BLOCK_EVICTED = 7
};

const int kMaxBlockSize = 4096 * 4;
const int16_t kMaxBlockFile = 255;
const int kMaxNumBlocks = 4;
const int16_t kFirstAdditionalBlockFile = 4;

// Defines a storage address for a cache record
//
// Header:
//   1000 0000 0000 0000 0000 0000 0000 0000 : initialized bit
//   0111 0000 0000 0000 0000 0000 0000 0000 : file type
//
// File type values:
//   0 = separate file on disk
//   1 = rankings block file
//   2 = 256 byte block file
//   3 = 1k byte block file
//   4 = 4k byte block file
//   5 = external files block file
//   6 = active entries block file
//   7 = evicted entries block file
//
// If separate file:
//   0000 1111 1111 1111 1111 1111 1111 1111 : file#  0 - 268,435,456 (2^28)
//
// If block file:
//   0000 1100 0000 0000 0000 0000 0000 0000 : reserved bits
//   0000 0011 0000 0000 0000 0000 0000 0000 : number of contiguous blocks 1-4
//   0000 0000 1111 1111 0000 0000 0000 0000 : file selector 0 - 255
//   0000 0000 0000 0000 1111 1111 1111 1111 : block#  0 - 65,535 (2^16)
//
// Note that an Addr can be used to "point" to a variety of different objects,
// from a given type of entry to random blobs of data. Conceptually, an Addr is
// just a number that someone can inspect to find out how to locate the desired
// record. Most users will not care about the specific bits inside Addr, for
// example, what parts of it point to a file number; only the code that has to
// select a specific file would care about those specific bits.
//
// From a general point of view, an Addr has a total capacity of 2^24 entities,
// in that it has 24 bits that can identify individual records. Note that the
// address space is bigger for independent files (2^28), but that would not be
// the general case.
class NET_EXPORT_PRIVATE Addr {
 public:
  Addr() : value_(0) {}
  explicit Addr(CacheAddr address) : value_(address) {}
  Addr(FileType file_type, int max_blocks, int block_file, int index) {
    value_ = ((file_type << kFileTypeOffset) & kFileTypeMask) |
             (((max_blocks - 1) << kNumBlocksOffset) & kNumBlocksMask) |
             ((block_file << kFileSelectorOffset) & kFileSelectorMask) |
             (index  & kStartBlockMask) | kInitializedMask;
  }

  CacheAddr value() const { return value_; }
  void set_value(CacheAddr address) {
    value_ = address;
  }

  bool is_initialized() const {
    return (value_ & kInitializedMask) != 0;
  }

  bool is_separate_file() const {
    return (value_ & kFileTypeMask) == 0;
  }

  bool is_block_file() const {
    return !is_separate_file();
  }

  FileType file_type() const {
    return static_cast<FileType>((value_ & kFileTypeMask) >> kFileTypeOffset);
  }

  int FileNumber() const {
    if (is_separate_file())
      return value_ & kFileNameMask;
    else
      return ((value_ & kFileSelectorMask) >> kFileSelectorOffset);
  }

  int start_block() const;
  int num_blocks() const;
  bool SetFileNumber(int file_number);
  int BlockSize() const {
    return BlockSizeForFileType(file_type());
  }

  bool operator==(Addr other) const {
    return value_ == other.value_;
  }

  bool operator!=(Addr other) const {
    return value_ != other.value_;
  }

  static int BlockSizeForFileType(FileType file_type) {
    switch (file_type) {
      case RANKINGS:
        return 36;
      case BLOCK_256:
        return 256;
      case BLOCK_1K:
        return 1024;
      case BLOCK_4K:
        return 4096;
      case BLOCK_FILES:
        return 8;
      case BLOCK_ENTRIES:
        return 104;
      case BLOCK_EVICTED:
        return 48;
      case EXTERNAL:
        NOTREACHED_IN_MIGRATION();
        return 0;
    }
    return 0;
  }

  static FileType RequiredFileType(int size) {
    if (size < 1024)
      return BLOCK_256;
    else if (size < 4096)
      return BLOCK_1K;
    else if (size <= 4096 * 4)
      return BLOCK_4K;
    else
      return EXTERNAL;
  }

  static int RequiredBlocks(int size, FileType file_type) {
    int block_size = BlockSizeForFileType(file_type);
    return (size + block_size - 1) / block_size;
  }

  // Returns true if this address looks like a valid one.
  bool SanityCheck() const;
  bool SanityCheckForEntry() const;
  bool SanityCheckForRankings() const;

 private:
  uint32_t reserved_bits() const { return value_ & kReservedBitsMask; }

  static const uint32_t kInitializedMask = 0x80000000;
  static const uint32_t kFileTypeMask = 0x70000000;
  static const uint32_t kFileTypeOffset = 28;
  static const uint32_t kReservedBitsMask = 0x0c000000;
  static const uint32_t kNumBlocksMask = 0x03000000;
  static const uint32_t kNumBlocksOffset = 24;
  static const uint32_t kFileSelectorMask = 0x00ff0000;
  static const uint32_t kFileSelectorOffset = 16;
  static const uint32_t kStartBlockMask = 0x0000FFFF;
  static const uint32_t kFileNameMask = 0x0FFFFFFF;

  CacheAddr value_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_ADDR_H_
