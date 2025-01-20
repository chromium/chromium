// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_IDENTITY_DIGEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_IDENTITY_DIGEST_H_

#include <optional>
#include <vector>

#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace WTF {
class SegmentedBuffer;
}  // namespace WTF

namespace blink {

class HTTPHeaderMap;

// Represent's a resource's `Identity-Digest` response headers.
//
// https://datatracker.ietf.org/doc/draft-pardue-http-identity-digest/
class PLATFORM_EXPORT IdentityDigest {
 public:
  ~IdentityDigest() = default;

  // Parses an HTTPHeaderMap, returning an `IdentityDigest` if a valid
  // `Identity-Digest` header is present, and `std::nullopt` otherwise.
  static std::optional<IdentityDigest> Create(const HTTPHeaderMap&);

  const WTF::HashSet<IntegrityMetadataPair>& digests() const {
    return integrity_metadata_.hashes;
  }

  // Validates |data| against all parsed digests, returning `true` if all match,
  // and `false` otherwise.
  bool DoesMatch(WTF::SegmentedBuffer* data);

 private:
  IdentityDigest() = default;
  explicit IdentityDigest(IntegrityMetadataSet);

  IntegrityMetadataSet integrity_metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_IDENTITY_DIGEST_H_
