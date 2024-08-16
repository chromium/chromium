// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_test_util.h"

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "net/base/hash_value.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_util.h"

namespace disk_cache::simple_util {

using base::File;
using base::FilePath;

bool CreateCorruptFileForTests(const std::string& key,
                               const FilePath& cache_path) {
  FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  int flags = File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE;
  File entry_file(entry_file_path, flags);

  if (!entry_file.IsValid())
    return false;

  return UNSAFE_TODO(entry_file.Write(0, "dummy", 1)) == 1;
}

bool RemoveKeySHA256FromEntry(const std::string& key,
                              const FilePath& cache_path) {
  FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  int flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
  File entry_file(entry_file_path, flags);
  if (!entry_file.IsValid())
    return false;
  int64_t file_length = entry_file.GetLength();
  SimpleFileEOF eof_record;
  if (file_length < static_cast<int64_t>(sizeof(eof_record)))
    return false;
  if (UNSAFE_TODO(entry_file.Read(file_length - sizeof(eof_record),
                                  reinterpret_cast<char*>(&eof_record),
                                  sizeof(eof_record))) != sizeof(eof_record)) {
    return false;
  }
  if (eof_record.final_magic_number != disk_cache::kSimpleFinalMagicNumber ||
      (eof_record.flags & SimpleFileEOF::FLAG_HAS_KEY_SHA256) !=
          SimpleFileEOF::FLAG_HAS_KEY_SHA256) {
    return false;
  }
  // Remove the key SHA256 flag, and rewrite the header on top of the
  // SHA256. Truncate the file afterwards, and we have an identical entry
  // lacking a key SHA256.
  eof_record.flags &= ~SimpleFileEOF::FLAG_HAS_KEY_SHA256;
  if (UNSAFE_TODO(entry_file.Write(
          file_length - sizeof(eof_record) - sizeof(net::SHA256HashValue),
          reinterpret_cast<char*>(&eof_record), sizeof(eof_record))) !=
      sizeof(eof_record)) {
    return false;
  }
  if (!entry_file.SetLength(file_length - sizeof(net::SHA256HashValue))) {
    return false;
  }
  return true;
}

bool CorruptKeySHA256FromEntry(const std::string& key,
                               const base::FilePath& cache_path) {
  FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  int flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
  File entry_file(entry_file_path, flags);
  if (!entry_file.IsValid())
    return false;
  int64_t file_length = entry_file.GetLength();
  SimpleFileEOF eof_record;
  if (file_length < static_cast<int64_t>(sizeof(eof_record)))
    return false;
  if (UNSAFE_TODO(entry_file.Read(file_length - sizeof(eof_record),
                                  reinterpret_cast<char*>(&eof_record),
                                  sizeof(eof_record))) != sizeof(eof_record)) {
    return false;
  }
  if (eof_record.final_magic_number != disk_cache::kSimpleFinalMagicNumber ||
      (eof_record.flags & SimpleFileEOF::FLAG_HAS_KEY_SHA256) !=
          SimpleFileEOF::FLAG_HAS_KEY_SHA256) {
    return false;
  }

  const char corrupt_data[] = "corrupt data";
  static_assert(sizeof(corrupt_data) <= sizeof(net::SHA256HashValue),
                "corrupt data should not be larger than a SHA-256");
  if (UNSAFE_TODO(entry_file.Write(
          file_length - sizeof(eof_record) - sizeof(net::SHA256HashValue),
          corrupt_data, sizeof(corrupt_data))) != sizeof(corrupt_data)) {
    return false;
  }
  return true;
}

bool CorruptStream0LengthFromEntry(const std::string& key,
                                   const base::FilePath& cache_path) {
  FilePath entry_file_path = cache_path.AppendASCII(
      disk_cache::simple_util::GetFilenameFromKeyAndFileIndex(key, 0));
  int flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
  File entry_file(entry_file_path, flags);
  if (!entry_file.IsValid())
    return false;
  int64_t file_length = entry_file.GetLength();
  SimpleFileEOF eof_record;
  if (file_length < static_cast<int64_t>(sizeof(eof_record)))
    return false;
  if (UNSAFE_TODO(entry_file.Read(file_length - sizeof(eof_record),
                                  reinterpret_cast<char*>(&eof_record),
                                  sizeof(eof_record))) != sizeof(eof_record)) {
    return false;
  }
  if (eof_record.final_magic_number != disk_cache::kSimpleFinalMagicNumber)
    return false;

  // Set the stream size to a clearly invalidly large value.
  eof_record.stream_size = std::numeric_limits<uint32_t>::max() - 50;
  if (UNSAFE_TODO(entry_file.Write(file_length - sizeof(eof_record),
                                   reinterpret_cast<char*>(&eof_record),
                                   sizeof(eof_record))) != sizeof(eof_record)) {
    return false;
  }
  return true;
}

}  // namespace disk_cache::simple_util
