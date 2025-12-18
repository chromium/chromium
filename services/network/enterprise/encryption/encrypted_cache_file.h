// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_

#include <memory>

#include "net/disk_cache/cache_file.h"

namespace network::enterprise_encryption {

// A decorator implementation of `CacheFile` that adds an encryption layer on
// top of another `CacheFile` instance.
// TODO(crbug.com/460509865): Currently a pass-through. Implement actual
// encryption/decryption logic.
class EncryptedCacheFile : public disk_cache::CacheFile {
 public:
  explicit EncryptedCacheFile(std::unique_ptr<disk_cache::CacheFile> file);
  ~EncryptedCacheFile() override;

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
  std::unique_ptr<disk_cache::CacheFile> file_;
};

}  // namespace network::enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_
