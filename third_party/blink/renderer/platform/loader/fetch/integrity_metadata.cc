// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"

#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

void IntegrityMetadataSet::Insert(IntegrityMetadata item) {
  switch (item.algorithm) {
    case IntegrityAlgorithm::kSha256:
    case IntegrityAlgorithm::kSha384:
    case IntegrityAlgorithm::kSha512:
      if (!hashes.Contains(item)) {
        hashes.push_back(std::move(item));
      }
      break;

    case IntegrityAlgorithm::kEd25519:
      if (!public_keys.Contains(item)) {
        public_keys.push_back(std::move(item));
      }
      break;
  }
}

}  // namespace blink
