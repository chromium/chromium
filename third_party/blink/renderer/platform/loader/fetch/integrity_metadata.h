// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_

#include "base/containers/enum_set.h"
#include "services/network/public/cpp/integrity_metadata.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using IntegrityAlgorithm = network::mojom::blink::IntegrityAlgorithm;
using IntegrityAlgorithmSet = base::EnumSet<IntegrityAlgorithm>;
using IntegrityMetadata = network::IntegrityMetadata;

// Contains the result of SRI's "Parse Metadata" algorithm:
//
// https://wicg.github.io/signature-based-sri/#abstract-opdef-parse-metadata
struct PLATFORM_EXPORT IntegrityMetadataSet {
  IntegrityMetadataSet() = default;
  bool empty() const { return hashes.empty() && public_keys.empty(); }
  Vector<IntegrityMetadata> hashes;
  Vector<IntegrityMetadata> public_keys;

  void Insert(IntegrityMetadata pair);

  bool operator==(const IntegrityMetadataSet& other) const {
    return this->hashes == other.hashes &&
           this->public_keys == other.public_keys;
  }
};

enum class ResourceIntegrityDisposition : uint8_t {
  kNotChecked = 0,
  kNetworkError,
  kFailedUnencodedDigest,
  kFailedIntegrityMetadata,
  kPassed
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
