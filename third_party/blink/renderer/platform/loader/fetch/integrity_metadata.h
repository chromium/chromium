// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class IntegrityMetadata;
enum class IntegrityAlgorithm : uint8_t;

using IntegrityMetadataPair = std::pair<String, IntegrityAlgorithm>;
using IntegrityMetadataSet = WTF::HashSet<IntegrityMetadataPair>;

class PLATFORM_EXPORT IntegrityMetadata {
  STACK_ALLOCATED();

 public:
  IntegrityMetadata() = default;
  IntegrityMetadata(String digest, IntegrityAlgorithm);
  IntegrityMetadata(IntegrityMetadataPair);

  String Digest() const { return digest_; }
  void SetDigest(const String& digest) { digest_ = digest; }
  IntegrityAlgorithm Algorithm() const { return algorithm_; }
  void SetAlgorithm(IntegrityAlgorithm algorithm) { algorithm_ = algorithm; }

  IntegrityMetadataPair ToPair() const;

  static bool SetsEqual(const IntegrityMetadataSet& set1,
                        const IntegrityMetadataSet& set2);

 private:
  String digest_;
  IntegrityAlgorithm algorithm_;
};

enum class ResourceIntegrityDisposition : uint8_t {
  kNotChecked = 0,
  kFailed,
  kPassed
};

enum class IntegrityAlgorithm : uint8_t { kSha256, kSha384, kSha512 };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_INTEGRITY_METADATA_H_
