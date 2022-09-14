// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_ENUMS_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_ENUMS_H_

namespace disk_cache {

// Used in histograms, please only add entries at the end.
enum OpenEntryResult {
  OPEN_ENTRY_SUCCESS = 0,
  OPEN_ENTRY_PLATFORM_FILE_ERROR = 1,
  OPEN_ENTRY_CANT_READ_HEADER = 2,
  OPEN_ENTRY_BAD_MAGIC_NUMBER = 3,
  OPEN_ENTRY_BAD_VERSION = 4,
  OPEN_ENTRY_CANT_READ_KEY = 5,
  OPEN_ENTRY_KEY_MISMATCH = 6,
  OPEN_ENTRY_KEY_HASH_MISMATCH = 7,
  OPEN_ENTRY_SPARSE_OPEN_FAILED = 8,
  OPEN_ENTRY_INVALID_FILE_LENGTH = 9,
  OPEN_ENTRY_MAX = 10,
};

// Used in histograms, please only add entries at the end.
enum OpenPrefetchMode {
  OPEN_PREFETCH_NONE = 0,
  OPEN_PREFETCH_FULL = 1,
  OPEN_PREFETCH_TRAILER = 2,
  OPEN_PREFETCH_MAX = 3,
};

// Used in histograms, please only add entries at the end.
enum SyncWriteResult {
  SYNC_WRITE_RESULT_SUCCESS = 0,
  SYNC_WRITE_RESULT_PRETRUNCATE_FAILURE = 1,
  SYNC_WRITE_RESULT_WRITE_FAILURE = 2,
  SYNC_WRITE_RESULT_TRUNCATE_FAILURE = 3,
  SYNC_WRITE_RESULT_LAZY_STREAM_ENTRY_DOOMED = 4,
  SYNC_WRITE_RESULT_LAZY_CREATE_FAILURE = 5,
  SYNC_WRITE_RESULT_LAZY_INITIALIZE_FAILURE = 6,
  SYNC_WRITE_RESULT_MAX = 7,
};

// Used in histograms, please only add entries at the end.
enum CheckEOFResult {
  CHECK_EOF_RESULT_SUCCESS = 0,
  CHECK_EOF_RESULT_READ_FAILURE = 1,
  CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH = 2,
  CHECK_EOF_RESULT_CRC_MISMATCH = 3,
  CHECK_EOF_RESULT_KEY_SHA256_MISMATCH = 4,
  CHECK_EOF_RESULT_MAX = 5,
};

// Used in histograms, please only add entries at the end.
enum CloseResult {
  CLOSE_RESULT_SUCCESS = 0,
  CLOSE_RESULT_WRITE_FAILURE = 1,
  CLOSE_RESULT_MAX = 2,
};

// Used in histograms, please only add entries at the end.
enum FileDescriptorLimiterOp {
  FD_LIMIT_CLOSE_FILE = 0,
  FD_LIMIT_REOPEN_FILE = 1,
  FD_LIMIT_FAIL_REOPEN_FILE = 2,
  FD_LIMIT_OP_MAX = 3
};

// This enumeration is used in histograms, add entries only at end.
enum OpenEntryIndexEnum {
  INDEX_NOEXIST = 0,
  INDEX_MISS = 1,
  INDEX_HIT = 2,
  INDEX_MAX = 3,
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_HISTOGRAM_ENUMS_H_
