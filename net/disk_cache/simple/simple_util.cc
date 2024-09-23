// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_util.h"

#include <string.h>

#include <limits>
#include <string_view>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/hash/sha1.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "third_party/zlib/zlib.h"

namespace {

// Size of the uint64_t hash_key number in Hex format in a string.
const size_t kEntryHashKeyAsHexStringSize = 2 * sizeof(uint64_t);

}  // namespace

namespace disk_cache::simple_util {

std::string ConvertEntryHashKeyToHexString(uint64_t hash_key) {
  const std::string hash_key_str = base::StringPrintf("%016" PRIx64, hash_key);
  DCHECK_EQ(kEntryHashKeyAsHexStringSize, hash_key_str.size());
  return hash_key_str;
}

std::string GetEntryHashKeyAsHexString(const std::string& key) {
  std::string hash_key_str =
      ConvertEntryHashKeyToHexString(GetEntryHashKey(key));
  DCHECK_EQ(kEntryHashKeyAsHexStringSize, hash_key_str.size());
  return hash_key_str;
}

bool GetEntryHashKeyFromHexString(std::string_view hash_key,
                                  uint64_t* hash_key_out) {
  if (hash_key.size() != kEntryHashKeyAsHexStringSize) {
    return false;
  }
  return base::HexStringToUInt64(hash_key, hash_key_out);
}

uint64_t GetEntryHashKey(const std::string& key) {
  base::SHA1Digest sha_hash = base::SHA1Hash(base::as_byte_span(key));
  return base::U64FromLittleEndian(base::span(sha_hash).first<8u>());
}

std::string GetFilenameFromEntryFileKeyAndFileIndex(
    const SimpleFileTracker::EntryFileKey& key,
    int file_index) {
  if (key.doom_generation == 0)
    return base::StringPrintf("%016" PRIx64 "_%1d", key.entry_hash, file_index);
  else
    return base::StringPrintf("todelete_%016" PRIx64 "_%1d_%" PRIu64,
                              key.entry_hash, file_index, key.doom_generation);
}

std::string GetSparseFilenameFromEntryFileKey(
    const SimpleFileTracker::EntryFileKey& key) {
  if (key.doom_generation == 0)
    return base::StringPrintf("%016" PRIx64 "_s", key.entry_hash);
  else
    return base::StringPrintf("todelete_%016" PRIx64 "_s_%" PRIu64,
                              key.entry_hash, key.doom_generation);
}

std::string GetFilenameFromKeyAndFileIndex(const std::string& key,
                                           int file_index) {
  return GetEntryHashKeyAsHexString(key) +
         base::StringPrintf("_%1d", file_index);
}

size_t GetHeaderSize(size_t key_length) {
  return sizeof(SimpleFileHeader) + key_length;
}

int32_t GetDataSizeFromFileSize(size_t key_length, int64_t file_size) {
  int64_t data_size =
      file_size - key_length - sizeof(SimpleFileHeader) - sizeof(SimpleFileEOF);
  return base::checked_cast<int32_t>(data_size);
}

int64_t GetFileSizeFromDataSize(size_t key_length, int32_t data_size) {
  return data_size + key_length + sizeof(SimpleFileHeader) +
         sizeof(SimpleFileEOF);
}

int GetFileIndexFromStreamIndex(int stream_index) {
  return (stream_index == 2) ? 1 : 0;
}

uint32_t Crc32(base::span<const uint8_t> data) {
  auto chars = base::as_chars(data);
  return Crc32(chars.data(), base::checked_cast<int>(data.size()));
}

uint32_t Crc32(const char* data, int length) {
  uint32_t empty_crc = crc32(0, Z_NULL, 0);
  if (length == 0)
    return empty_crc;
  return crc32(empty_crc, reinterpret_cast<const Bytef*>(data), length);
}

uint32_t IncrementalCrc32(uint32_t previous_crc, const char* data, int length) {
  return crc32(previous_crc, reinterpret_cast<const Bytef*>(data), length);
}

}  // namespace disk_cache::simple_util
