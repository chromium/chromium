// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_FILE_H_
#define NET_DISK_CACHE_CACHE_FILE_H_

#include <optional>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "net/base/net_export.h"

namespace disk_cache {

// An interface that abstracts file operations for the disk cache. This provides
// the subset of `base::File` operations required by the cache backend, allowing
// different implementations to be used interchangeably through decoration.
class NET_EXPORT CacheFile {
 public:
  virtual ~CacheFile() = default;

  virtual bool IsValid() const = 0;
  virtual base::File::Error error_details() const = 0;

  virtual std::optional<size_t> Read(int64_t offset,
                                     base::span<uint8_t> data) = 0;
  virtual std::optional<size_t> Write(int64_t offset,
                                      base::span<const uint8_t> data) = 0;

  virtual bool GetInfo(base::File::Info* file_info) = 0;
  virtual int64_t GetLength() = 0;
  virtual bool SetLength(int64_t length) = 0;

  virtual bool ReadAndCheck(int64_t offset, base::span<uint8_t> data) = 0;
  virtual bool WriteAndCheck(int64_t offset,
                             base::span<const uint8_t> data) = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_FILE_H_
