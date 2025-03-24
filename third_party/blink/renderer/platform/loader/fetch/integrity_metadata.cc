// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"

namespace blink {

void IntegrityMetadataSet::Insert(const IntegrityMetadataPair& pair) {
  switch (pair.second) {
    case IntegrityAlgorithm::kSha256:
    case IntegrityAlgorithm::kSha384:
    case IntegrityAlgorithm::kSha512:
      if (!hashes.Contains(pair)) {
        hashes.push_back(std::move(pair));
      }
      break;

    case IntegrityAlgorithm::kEd25519:
      if (!public_keys.Contains(pair)) {
        public_keys.push_back(std::move(pair));
      }
      break;
  }
}

IntegrityMetadata::IntegrityMetadata(WTF::String digest,
                                     IntegrityAlgorithm algorithm)
    : digest_(digest), algorithm_(algorithm) {}

IntegrityMetadata::IntegrityMetadata(IntegrityMetadataPair pair)
    : digest_(pair.first), algorithm_(pair.second) {}

IntegrityMetadataPair IntegrityMetadata::ToPair() const {
  return IntegrityMetadataPair(digest_, algorithm_);
}

}  // namespace blink
