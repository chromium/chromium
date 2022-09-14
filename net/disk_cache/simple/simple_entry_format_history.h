// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_HISTORY_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_HISTORY_H_

#include <stdint.h>

#include "net/base/net_export.h"

namespace disk_cache::simplecache_v5 {

const uint64_t kSimpleInitialMagicNumber = UINT64_C(0xfcfb6d1ba7725c30);
const uint64_t kSimpleFinalMagicNumber = UINT64_C(0xf4fa6f45970d41d8);

// A file containing stream 0 and stream 1 in the Simple cache consists of:
//   - a SimpleFileHeader.
//   - the key.
//   - the data from stream 1.
//   - a SimpleFileEOF record for stream 1.
//   - the data from stream 0.
//   - a SimpleFileEOF record for stream 0.

// A file containing stream 2 in the Simple cache consists of:
//   - a SimpleFileHeader.
//   - the key.
//   - the data.
//   - at the end, a SimpleFileEOF record.
static const int kSimpleEntryFileCount = 2;
static const int kSimpleEntryStreamCount = 3;

struct NET_EXPORT_PRIVATE SimpleFileHeader {
  SimpleFileHeader();

  uint64_t initial_magic_number;
  uint32_t version;
  uint32_t key_length;
  uint32_t key_hash;
};

struct NET_EXPORT_PRIVATE SimpleFileEOF {
  enum Flags {
    FLAG_HAS_CRC32 = (1U << 0),
  };

  SimpleFileEOF();

  uint64_t final_magic_number;
  uint32_t flags;
  uint32_t data_crc32;
  // |stream_size| is only used in the EOF record for stream 0.
  uint32_t stream_size;
};

}  // namespace disk_cache::simplecache_v5

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_HISTORY_H_
