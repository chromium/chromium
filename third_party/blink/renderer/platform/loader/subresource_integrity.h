// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class KURL;
class Resource;

class PLATFORM_EXPORT SubresourceIntegrity final {
  STATIC_ONLY(SubresourceIntegrity);

 public:
  class PLATFORM_EXPORT ReportInfo final {
    DISALLOW_NEW();

   public:
    enum class UseCounterFeature {
      kSRIElementWithMatchingIntegrityAttribute,
      kSRIElementWithNonMatchingIntegrityAttribute,
      kSRIElementIntegrityAttributeButIneligible,
      kSRIElementWithUnparsableIntegrityAttribute,
      kSRISignatureCheck,
      kSRISignatureSuccess,
    };

    void AddUseCount(UseCounterFeature);
    void AddConsoleErrorMessage(const String&);
    void Clear();

    const Vector<UseCounterFeature>& UseCounts() const { return use_counts_; }
    const Vector<String>& ConsoleErrorMessages() const {
      return console_error_messages_;
    }

   private:
    Vector<UseCounterFeature> use_counts_;
    Vector<String> console_error_messages_;
  };

  // Determine which SRI features to support when parsing integrity attributes.
  enum class IntegrityFeatures {
    kDefault,    // Default: All sha* hash codes.
    kSignatures  // Also support the ed25519 signature scheme.
  };

  // The version with the IntegrityMetadataSet passed as the first argument
  // assumes that the integrity attribute has already been parsed, and the
  // IntegrityMetadataSet represents the result of that parsing.
  // Edge case: If a resource has zero bytes then this method is called with a
  // null `buffer`.
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const SegmentedBuffer* buffer,
                                        const KURL& resource_url,
                                        const Resource&,
                                        ReportInfo&);
  static bool CheckSubresourceIntegrity(const String&,
                                        IntegrityFeatures,
                                        const SegmentedBuffer* buffer,
                                        const KURL& resource_url,
                                        ReportInfo&);

  // The IntegrityMetadataSet arguments are out parameters which contain the
  // set of all valid, parsed metadata from |attribute|.
  static void ParseIntegrityAttribute(const WTF::String& attribute,
                                      IntegrityFeatures,
                                      IntegrityMetadataSet&);
  static void ParseIntegrityAttribute(const WTF::String& attribute,
                                      IntegrityFeatures,
                                      IntegrityMetadataSet&,
                                      ReportInfo*);

 private:
  friend class SubresourceIntegrityTest;
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Parsing);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Prioritization);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, FindBestAlgorithm);

  // The core implementation for all CheckSubresoureIntegrity functions.
  static bool CheckSubresourceIntegrityImpl(const IntegrityMetadataSet&,
                                            const SegmentedBuffer* buffer,
                                            const KURL& resource_url,
                                            ReportInfo&);

  enum AlgorithmParseResult {
    kAlgorithmValid,
    kAlgorithmUnparsable,
    kAlgorithmUnknown
  };

  static IntegrityAlgorithm FindBestAlgorithm(const IntegrityMetadataSet&);

  typedef bool (*CheckFunction)(const IntegrityMetadata&,
                                const char*,
                                size_t,
                                const String&);

  static bool CheckSubresourceIntegrityDigest(const IntegrityMetadata&,
                                              const SegmentedBuffer* buffer);
  static bool CheckSubresourceIntegritySignature(const IntegrityMetadata&,
                                                 const char*,
                                                 size_t);

  static AlgorithmParseResult ParseAttributeAlgorithm(const UChar*& begin,
                                                      const UChar* end,
                                                      IntegrityFeatures,
                                                      IntegrityAlgorithm&);
  typedef std::pair<const char*, IntegrityAlgorithm> AlgorithmPrefixPair;
  static AlgorithmParseResult ParseAlgorithmPrefix(
      const UChar*& string_position,
      const UChar* string_end,
      const AlgorithmPrefixPair* prefix_table,
      size_t prefix_table_size,
      IntegrityAlgorithm&);
  static bool ParseDigest(const UChar*& begin,
                          const UChar* end,
                          String& digest);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_
