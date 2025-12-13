// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_cache_file.h"

#include <utility>

namespace network::enterprise {

EncryptedCacheFile::EncryptedCacheFile(
    std::unique_ptr<disk_cache::CacheFile> file)
    : file_(std::move(file)) {}

EncryptedCacheFile::~EncryptedCacheFile() = default;

bool EncryptedCacheFile::IsValid() const {
  return file_->IsValid();
}

base::File::Error EncryptedCacheFile::error_details() const {
  return file_->error_details();
}

std::optional<size_t> EncryptedCacheFile::Read(int64_t offset,
                                               base::span<uint8_t> data) {
  return file_->Read(offset, data);
}

std::optional<size_t> EncryptedCacheFile::Write(
    int64_t offset,
    base::span<const uint8_t> data) {
  return file_->Write(offset, data);
}

bool EncryptedCacheFile::GetInfo(base::File::Info* file_info) {
  return file_->GetInfo(file_info);
}

int64_t EncryptedCacheFile::GetLength() {
  return file_->GetLength();
}

bool EncryptedCacheFile::SetLength(int64_t length) {
  return file_->SetLength(length);
}

bool EncryptedCacheFile::ReadAndCheck(int64_t offset,
                                      base::span<uint8_t> data) {
  return file_->ReadAndCheck(offset, data);
}

bool EncryptedCacheFile::WriteAndCheck(int64_t offset,
                                       base::span<const uint8_t> data) {
  return file_->WriteAndCheck(offset, data);
}

}  // namespace network::enterprise
