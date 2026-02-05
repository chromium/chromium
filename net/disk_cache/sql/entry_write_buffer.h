// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_ENTRY_WRITE_BUFFER_H_
#define NET_DISK_CACHE_SQL_ENTRY_WRITE_BUFFER_H_

#include <cstdint>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

// Buffers data for stream 1 writes.
struct NET_EXPORT_PRIVATE EntryWriteBuffer {
  EntryWriteBuffer();
  EntryWriteBuffer(scoped_refptr<net::IOBuffer> buffer,
                   int size,
                   int64_t offset);
  ~EntryWriteBuffer();

  EntryWriteBuffer(const EntryWriteBuffer&) = delete;
  EntryWriteBuffer& operator=(const EntryWriteBuffer&) = delete;

  EntryWriteBuffer(EntryWriteBuffer&&);
  EntryWriteBuffer& operator=(EntryWriteBuffer&&);

  std::vector<scoped_refptr<net::IOBuffer>> buffers;
  int size = 0;
  int64_t offset = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_ENTRY_WRITE_BUFFER_H_
