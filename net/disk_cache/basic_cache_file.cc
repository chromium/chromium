// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/basic_cache_file.h"

namespace disk_cache {

BasicCacheFile::BasicCacheFile(base::File file) : file_(std::move(file)) {}

BasicCacheFile::~BasicCacheFile() = default;

bool BasicCacheFile::IsValid() const {
  return file_.IsValid();
}

base::File::Error BasicCacheFile::error_details() const {
  return file_.error_details();
}

std::optional<size_t> BasicCacheFile::Read(int64_t offset,
                                           base::span<uint8_t> data) {
  return file_.Read(offset, data);
}

std::optional<size_t> BasicCacheFile::Write(int64_t offset,
                                            base::span<const uint8_t> data) {
  return file_.Write(offset, data);
}

bool BasicCacheFile::GetInfo(base::File::Info* file_info) {
  return file_.GetInfo(file_info);
}

int64_t BasicCacheFile::GetLength() {
  return file_.GetLength();
}

bool BasicCacheFile::SetLength(int64_t length) {
  return file_.SetLength(length);
}

bool BasicCacheFile::ReadAndCheck(int64_t offset, base::span<uint8_t> data) {
  return file_.ReadAndCheck(offset, data);
}

bool BasicCacheFile::WriteAndCheck(int64_t offset,
                                   base::span<const uint8_t> data) {
  return file_.WriteAndCheck(offset, data);
}

}  // namespace disk_cache
