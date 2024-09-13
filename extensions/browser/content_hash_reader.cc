// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_reader.h"

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/verified_contents.h"

namespace extensions {

ContentHashReader::ContentHashReader(InitStatus status) : status_(status) {}

ContentHashReader::~ContentHashReader() = default;

// static
std::unique_ptr<const ContentHashReader> ContentHashReader::Create(
    const base::FilePath& relative_path,
    const scoped_refptr<const ContentHash>& content_hash) {
  ComputedHashes::Status hashes_status = content_hash->computed_hashes_status();
  if (hashes_status == ComputedHashes::Status::UNKNOWN ||
      hashes_status == ComputedHashes::Status::READ_FAILED) {
    // Failure: no hashes at all.
    return base::WrapUnique(new ContentHashReader(InitStatus::HASHES_MISSING));
  }
  if (hashes_status == ComputedHashes::Status::PARSE_FAILED) {
    // Failure: hashes are unreadable.
    return base::WrapUnique(new ContentHashReader(InitStatus::HASHES_DAMAGED));
  }
  DCHECK_EQ(ComputedHashes::Status::SUCCESS, hashes_status);

  const ComputedHashes& computed_hashes = content_hash->computed_hashes();
  std::optional<std::string> root;

  int block_size;
  std::vector<std::string> block_hashes;

  if (computed_hashes.GetHashes(relative_path, &block_size, &block_hashes) &&
      block_size % crypto::kSHA256Length == 0) {
    root =
        ComputeTreeHashRoot(block_hashes, block_size / crypto::kSHA256Length);
  }

  ContentHash::TreeHashVerificationResult verification =
      content_hash->VerifyTreeHashRoot(relative_path,
                                       base::OptionalToPtr(root));
  switch (verification) {
    case ContentHash::TreeHashVerificationResult::SUCCESS: {
      auto hash_reader =
          base::WrapUnique(new ContentHashReader(InitStatus::SUCCESS));
      hash_reader->block_size_ = block_size;
      hash_reader->hashes_ = std::move(block_hashes);
      return hash_reader;
    }
    case ContentHash::TreeHashVerificationResult::NO_ENTRY: {
      return base::WrapUnique(
          new ContentHashReader(InitStatus::NO_HASHES_FOR_RESOURCE));
    }
    case ContentHash::TreeHashVerificationResult::HASH_MISMATCH: {
      return base::WrapUnique(
          new ContentHashReader(InitStatus::HASHES_DAMAGED));
    }
  }
}

int ContentHashReader::block_count() const {
  return hashes_.size();
}

int ContentHashReader::block_size() const {
  return block_size_;
}

bool ContentHashReader::GetHashForBlock(int block_index,
                                        const std::string** result) const {
  if (status_ != InitStatus::SUCCESS) {
    return false;
  }
  DCHECK(block_index >= 0);

  if (static_cast<unsigned>(block_index) >= hashes_.size()) {
    return false;
  }
  *result = &hashes_[block_index];

  return true;
}

}  // namespace extensions
