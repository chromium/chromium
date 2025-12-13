// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BASIC_CACHE_FILE_H_
#define NET_DISK_CACHE_BASIC_CACHE_FILE_H_

#include <optional>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "net/base/net_export.h"
#include "net/disk_cache/cache_file.h"

namespace disk_cache {

// Default implementation of `CacheFile` that wraps a `base::File`.
// Delegates operations directly to it.
class NET_EXPORT BasicCacheFile : public CacheFile {
 public:
  explicit BasicCacheFile(base::File file);
  ~BasicCacheFile() override;

  bool IsValid() const override;
  base::File::Error error_details() const override;

  std::optional<size_t> Read(int64_t offset, base::span<uint8_t> data) override;
  std::optional<size_t> Write(int64_t offset,
                              base::span<const uint8_t> data) override;

  bool GetInfo(base::File::Info* file_info) override;
  int64_t GetLength() override;
  bool SetLength(int64_t length) override;

  bool ReadAndCheck(int64_t offset, base::span<uint8_t> data) override;
  bool WriteAndCheck(int64_t offset, base::span<const uint8_t> data) override;

 private:
  base::File file_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BASIC_CACHE_FILE_H_
