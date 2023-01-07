// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The cache is stored on disk as a collection of block-files, plus an index
// file plus a collection of external files.
//
// Any data blob bigger than kMaxBlockSize (disk_cache/addr.h) will be stored in
// a separate file named f_xxx where x is a hexadecimal number. Shorter data
// will be stored as a series of blocks on a block-file. In any case, CacheAddr
// represents the address of the data inside the cache.
//
// The index file is just a simple hash table that maps a particular entry to
// a CacheAddr value. Linking for a given hash bucket is handled internally
// by the cache entry.
//
// The last element of the cache is the block-file. A block file is a file
// designed to store blocks of data of a given size. For more details see
// disk_cache/disk_format_base.h
//
// A new cache is initialized with four block files (named data_0 through
// data_3), each one dedicated to store blocks of a given size. The number at
// the end of the file name is the block file number (in decimal).
//
// There are two "special" types of blocks: an entry and a rankings node. An
// entry keeps track of all the information related to the same cache entry,
// such as the key, hash value, data pointers etc. A rankings node keeps track
// of the information that is updated frequently for a given entry, such as its
// location on the LRU lists, last access time etc.
//
// The files that store internal information for the cache (blocks and index)
// are at least partially memory mapped. They have a location that is signaled
// every time the internal structures are modified, so it is possible to detect
// (most of the time) when the process dies in the middle of an update.
//
// In order to prevent dirty data to be used as valid (after a crash), every
// cache entry has a dirty identifier. Each running instance of the cache keeps
// a separate identifier (maintained on the "this_id" header field) that is used
// to mark every entry that is created or modified. When the entry is closed,
// and all the data can be trusted, the dirty flag is cleared from the entry.
// When the cache encounters an entry whose identifier is different than the one
// being currently used, it means that the entry was not properly closed on a
// previous run, so it is discarded.

#ifndef NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_H_
#define NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/disk_format_base.h"

namespace disk_cache {

const int kIndexTablesize = 0x10000;
const uint32_t kIndexMagic = 0xC103CAC3;
const uint32_t kVersion2_0 = 0x20000;
const uint32_t kVersion2_1 = 0x20001;
const uint32_t kVersion3_0 = 0x30000;
const uint32_t kCurrentVersion = kVersion3_0;

struct LruData {
  int32_t pad1[2];
  int32_t filled;  // Flag to tell when we filled the cache.
  int32_t sizes[5];
  CacheAddr heads[5];
  CacheAddr tails[5];
  CacheAddr transaction;     // In-flight operation target.
  int32_t operation;         // Actual in-flight operation.
  int32_t operation_list;    // In-flight operation list.
  int32_t pad2[7];
};

// Header for the master index file.
struct NET_EXPORT_PRIVATE IndexHeader {
  IndexHeader();

  uint32_t magic;
  uint32_t version;
  int32_t num_entries;       // Number of entries currently stored.
  int32_t old_v2_num_bytes;  // Total size of the stored data, in versions 2.x
  int32_t last_file;         // Last external file created.
  int32_t this_id;           // Id for all entries being changed (dirty flag).
  CacheAddr   stats;         // Storage for usage data.
  int32_t table_len;         // Actual size of the table (0 == kIndexTablesize).
  int32_t crash;             // Signals a previous crash.
  int32_t experiment;        // Id of an ongoing test.
  uint64_t create_time;      // Creation time for this set of files.
  int64_t num_bytes;         // Total size of the stored data, in version 3.0
  int32_t pad[50];
  LruData     lru;           // Eviction control data.
};

// The structure of the whole index file.
struct Index {
  IndexHeader header;
  CacheAddr   table[kIndexTablesize];  // Default size. Actual size controlled
                                       // by header.table_len.
};

// Main structure for an entry on the backing storage. If the key is longer than
// what can be stored on this structure, it will be extended on consecutive
// blocks (adding 256 bytes each time), up to 4 blocks (1024 - 32 - 1 chars).
// After that point, the whole key will be stored as a data block or external
// file.
struct EntryStore {
  uint32_t hash;                  // Full hash of the key.
  CacheAddr   next;               // Next entry with the same hash or bucket.
  CacheAddr   rankings_node;      // Rankings node for this entry.
  int32_t reuse_count;            // How often is this entry used.
  int32_t refetch_count;          // How often is this fetched from the net.
  int32_t state;                  // Current state.
  uint64_t creation_time;
  int32_t key_len;
  CacheAddr   long_key;           // Optional address of a long key.
  int32_t data_size[4];           // We can store up to 4 data streams for each
  CacheAddr   data_addr[4];       // entry.
  uint32_t flags;                 // Any combination of EntryFlags.
  int32_t pad[4];
  uint32_t self_hash;             // The hash of EntryStore up to this point.
  char        key[256 - 24 * 4];  // null terminated
};

static_assert(sizeof(EntryStore) == 256, "bad EntryStore");
const int kMaxInternalKeyLength = 4 * sizeof(EntryStore) -
                                  offsetof(EntryStore, key) - 1;

// Possible states for a given entry.
enum EntryState {
  ENTRY_NORMAL = 0,
  ENTRY_EVICTED,    // The entry was recently evicted from the cache.
  ENTRY_DOOMED      // The entry was doomed.
};

// Flags that can be applied to an entry.
enum EntryFlags {
  PARENT_ENTRY = 1,         // This entry has children (sparse) entries.
  CHILD_ENTRY = 1 << 1      // Child entry that stores sparse data.
};

#pragma pack(push, 4)
// Rankings information for a given entry.
struct RankingsNode {
  uint64_t last_used;           // LRU info.
  uint64_t last_modified;       // LRU info.
  CacheAddr   next;             // LRU list.
  CacheAddr   prev;             // LRU list.
  CacheAddr   contents;         // Address of the EntryStore.
  int32_t dirty;                // The entry is being modifyied.
  uint32_t self_hash;           // RankingsNode's hash.
};
#pragma pack(pop)

static_assert(sizeof(RankingsNode) == 36, "bad RankingsNode");

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_H_
