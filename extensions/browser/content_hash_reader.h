// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_
#define EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ContentHash;

// This class creates an object that will read expected hashes that may have
// been fetched/calculated by the ContentHashFetcher, and vends them out for
// use in ContentVerifyJob's.
class ContentHashReader {
 public:
  enum class InitStatus {
    // Extension has no hashes for resources verification.
    HASHES_MISSING,

    // Extension has hashes files, but they are unreadable or corrupted.
    HASHES_DAMAGED,

    // Resource doesn't have entry in hashes.
    NO_HASHES_FOR_RESOURCE,

    // Ready to verify resource's content.
    SUCCESS
  };

  ContentHashReader(const ContentHashReader&) = delete;
  ContentHashReader& operator=(const ContentHashReader&) = delete;

  ~ContentHashReader();

  // Factory to create ContentHashReader to get expected hashes for the file at
  // |relative_path| within an extension.
  // Must be called on a thread that is allowed to do file I/O. Returns an
  // instance whose success or failure type can be determined by calling
  // status() method. On failure, this object should likely be discarded.
  static std::unique_ptr<const ContentHashReader> Create(
      const base::FilePath& relative_path,
      const scoped_refptr<const ContentHash>& content_hash);

  InitStatus status() const { return status_; }

  // Return the number of blocks and block size, respectively. Only valid after
  // calling Init().
  int block_count() const;
  int block_size() const;

  // Returns a pointer to the expected sha256 hash value for the block at the
  // given index. Only valid after calling Init().
  bool GetHashForBlock(int block_index, const std::string** result) const;

 private:
  explicit ContentHashReader(InitStatus status);

  InitStatus status_;

  // The blocksize used for generating the hashes.
  int block_size_ = 0;

  std::vector<std::string> hashes_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_HASH_READER_H_
