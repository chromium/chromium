// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_
#define EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"

namespace extensions {

class ContentHash;

// The expected hashes for a single file.
struct ContentHashData {
  ContentHashData(int block_size, std::vector<std::string> hashes);
  ~ContentHashData();

  ContentHashData(const ContentHashData&) = delete;
  ContentHashData& operator=(const ContentHashData&) = delete;

  ContentHashData(ContentHashData&&);
  ContentHashData& operator=(ContentHashData&&);

  int block_size;
  std::vector<std::string> hashes;
};

// Represents the failure status of fetching expected hashes for a file.
enum class ContentHashReaderInitStatus {
  // Extension has no hashes for resources verification.
  HASHES_MISSING,

  // Extension has hashes files, but they are unreadable or corrupted.
  HASHES_DAMAGED,

  // Resource doesn't have entry in hashes.
  NO_HASHES_FOR_RESOURCE,

  // Extension has not attempted to fetch hashes yet.
  FETCH_NOT_ATTEMPTED_YET,
};

// Reads expected hashes that may have been fetched/calculated by the
// ContentHashFetcher, and vends them out for use in ContentVerifyJob's.
// Returns the expected hashes for the file at `relative_path` within an
// extension, or an error status on failure.
base::expected<ContentHashData, ContentHashReaderInitStatus> ReadContentHashes(
    const base::FilePath& relative_path,
    const scoped_refptr<const ContentHash>& content_hash);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_
