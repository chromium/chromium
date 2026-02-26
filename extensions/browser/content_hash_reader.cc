// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_reader.h"

#include "base/types/optional_util.h"
#include "crypto/hash.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_hash.h"

namespace extensions {

ContentHashData::ContentHashData(int block_size,
                                 std::vector<std::string> hashes)
    : block_size(block_size), hashes(std::move(hashes)) {}
ContentHashData::~ContentHashData() = default;

ContentHashData::ContentHashData(ContentHashData&&) = default;
ContentHashData& ContentHashData::operator=(ContentHashData&&) = default;

base::expected<ContentHashData, ContentHashReaderInitStatus> ReadContentHashes(
    const base::FilePath& relative_path,
    const scoped_refptr<const ContentHash>& content_hash) {
  ComputedHashes::Status hashes_status = content_hash->computed_hashes_status();
  if (hashes_status == ComputedHashes::Status::UNKNOWN ||
      hashes_status == ComputedHashes::Status::READ_FAILED) {
    // Failure: no hashes at all.
    return base::unexpected(ContentHashReaderInitStatus::HASHES_MISSING);
  }
  if (hashes_status == ComputedHashes::Status::PARSE_FAILED) {
    // Failure: hashes are unreadable.
    return base::unexpected(ContentHashReaderInitStatus::HASHES_DAMAGED);
  }
  DCHECK_EQ(ComputedHashes::Status::SUCCESS, hashes_status);

  const ComputedHashes& computed_hashes = content_hash->computed_hashes();
  std::optional<std::string> root;

  int block_size;
  std::vector<std::string> block_hashes;

  if (computed_hashes.GetHashes(relative_path, &block_size, &block_hashes) &&
      block_size % crypto::hash::kSha256Size == 0) {
    root = ComputeTreeHashRoot(block_hashes,
                               block_size / crypto::hash::kSha256Size);
  }

  ContentHash::TreeHashVerificationResult verification =
      content_hash->VerifyTreeHashRoot(relative_path,
                                       base::OptionalToPtr(root));
  switch (verification) {
    case ContentHash::TreeHashVerificationResult::SUCCESS:
      return ContentHashData(block_size, std::move(block_hashes));
    case ContentHash::TreeHashVerificationResult::NO_ENTRY:
      return base::unexpected(
          ContentHashReaderInitStatus::NO_HASHES_FOR_RESOURCE);
    case ContentHash::TreeHashVerificationResult::HASH_MISMATCH:
      return base::unexpected(ContentHashReaderInitStatus::HASHES_DAMAGED);
  }
}

}  // namespace extensions
