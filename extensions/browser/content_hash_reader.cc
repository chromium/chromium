// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_reader.h"

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/verified_contents.h"

namespace extensions {

ContentHashReader::ContentHashReader() {}

ContentHashReader::~ContentHashReader() {}

// static.
std::unique_ptr<const ContentHashReader> ContentHashReader::Create(
    const base::FilePath& relative_path,
    const scoped_refptr<const ContentHash>& content_hash) {
  base::ElapsedTimer timer;

  const ContentHash::ExtensionKey& extension_key =
      content_hash->extension_key();
  auto hash_reader = base::WrapUnique(new ContentHashReader);

  if (!content_hash->succeeded())
    return hash_reader;  // FAILURE.

  hash_reader->has_content_hashes_ = true;

  const VerifiedContents& verified_contents = content_hash->verified_contents();

  // Extensions sometimes request resources that do not have an entry in
  // verified_contents.json. This can happen when an extension sends an XHR to a
  // resource.
  if (!verified_contents.HasTreeHashRoot(relative_path)) {
    base::FilePath full_path =
        extension_key.extension_root.Append(relative_path);
    // Making a request to a non-existent file or to a directory should not
    // result in content verification failure.
    // TODO(proberge): This logic could be simplified if |content_verify_job|
    // kept track of whether the file being verified was successfully read.
    // A content verification failure should be triggered if there is a mismatch
    // between the file read state and the existence of verification hashes.
    if (!base::PathExists(full_path) || base::DirectoryExists(full_path))
      hash_reader->file_missing_from_verified_contents_ = true;

    return hash_reader;  // FAILURE.
  }

  const ComputedHashes::Reader& reader = content_hash->computed_hashes();
  if (!reader.GetHashes(relative_path, &hash_reader->block_size_,
                        &hash_reader->hashes_) ||
      hash_reader->block_size_ % crypto::kSHA256Length != 0) {
    return hash_reader;
  }

  std::string root = ComputeTreeHashRoot(
      hash_reader->hashes_, hash_reader->block_size_ / crypto::kSHA256Length);
  if (!verified_contents.TreeHashRootEquals(relative_path, root))
    return hash_reader;

  hash_reader->status_ = SUCCESS;
  UMA_HISTOGRAM_TIMES("ExtensionContentHashReader.InitLatency",
                      timer.Elapsed());
  return hash_reader;  // SUCCESS.
}

int ContentHashReader::block_count() const {
  return hashes_.size();
}

int ContentHashReader::block_size() const {
  return block_size_;
}

bool ContentHashReader::GetHashForBlock(int block_index,
                                        const std::string** result) const {
  if (status_ != SUCCESS)
    return false;
  DCHECK(block_index >= 0);

  if (static_cast<unsigned>(block_index) >= hashes_.size())
    return false;
  *result = &hashes_[block_index];

  return true;
}

}  // namespace extensions
