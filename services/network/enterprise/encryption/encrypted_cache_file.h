// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_

#include <memory>

#include "base/component_export.h"
#include "crypto/process_bound_string.h"
#include "net/disk_cache/cache_file.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/enterprise/encryption/chunked_encryptor.h"

namespace network::enterprise_encryption {

// A decorator implementation of `CacheFile` that adds an encryption layer on
// top of another `CacheFile` instance.
class COMPONENT_EXPORT(NETWORK_SERVICE) EncryptedCacheFile
    : public disk_cache::CacheFile {
 public:
  EncryptedCacheFile(std::unique_ptr<disk_cache::CacheFile> file,
                     const crypto::ProcessBoundString& primary_key);

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
  // Checks lazily that the file is initialized (header read/written,
  // encryptor created).
  bool EnsureInitialized();

  // Helper to write data into a specific chunk.
  // Handles partial updates (Read-Modify-Write) and new chunk creation.
  // |is_new_chunk|: For optimization purposes. If true, assumes the chunk is
  // currently empty/non-existent. |is_last_chunk|: Encrypts the chunk with the
  // "last chunk" flag in the nonce.
  bool WriteChunk(uint32_t chunk_index,
                  size_t offset_in_chunk,
                  base::span<const uint8_t> data_to_write,
                  bool is_new_chunk,
                  bool is_last_chunk);

  // Reads and decrypts the specified chunk.
  base::expected<std::vector<uint8_t>, EncryptionError> ReadAndDecryptChunk(
      uint32_t chunk_index);

  // Handles the transition of the previous last chunk to an intermediate chunk.
  // When extending the file, the old "last chunk" must be padded to the full
  // chunk size and re-encrypted with `is_last_chunk=false` to allow subsequent
  // chunks to be valid.
  bool EnsurePreviousChunkNotLast(int64_t new_logical_length);

  std::unique_ptr<disk_cache::CacheFile> file_;
  const crypto::ProcessBoundString key_;
  std::unique_ptr<ChunkedEncryptor> encryptor_;
  bool initialized_ = false;
};

}  // namespace network::enterprise_encryption

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_CACHE_FILE_H_
