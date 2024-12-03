// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_

#include "base/gtest_prod_util.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class IntegrityReport;
class KURL;
class Resource;

using network::mojom::blink::FetchResponseType;

class PLATFORM_EXPORT SubresourceIntegrity final {
  STATIC_ONLY(SubresourceIntegrity);

 public:
  // Check the integrity of a given |buffer|'s content against the metadata in
  // the `IntegrityMetadataSet` provided.
  //
  // Either a `Resource` or `FetchResponseType` will be used to validate the
  // response's eligibility for client-initiated integrity checks. Ineligible
  // resources will fail the check.
  //
  // Note: If a resource's body is empty, then these methods are called with a
  // null `buffer`.
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const SegmentedBuffer* buffer,
                                        const KURL& resource_url,
                                        const Resource&,
                                        IntegrityReport&);
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const SegmentedBuffer* buffer,
                                        const KURL& resource_url,
                                        const FetchResponseType,
                                        const String& raw_headers,
                                        IntegrityReport&);

  // The IntegrityMetadataSet argument is an out parameters which contains the
  // set of all valid, parsed metadata from |attribute|.
  static void ParseIntegrityAttribute(const WTF::String& attribute,
                                      IntegrityMetadataSet&);
  static void ParseIntegrityAttribute(const WTF::String& attribute,
                                      IntegrityMetadataSet&,
                                      IntegrityReport*);

 private:
  friend class SubresourceIntegrityTest;
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Parsing);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseSignatureAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest,
                           AlgorithmEnumPrioritization);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, FindBestAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegritySignatureTest,
                           ParseSignatureAlgorithm);

  // The core implementation for all CheckSubresoureIntegrity functions.
  static bool CheckSubresourceIntegrityImpl(const IntegrityMetadataSet&,
                                            const SegmentedBuffer* buffer,
                                            const KURL& resource_url,
                                            const String& raw_headers,
                                            IntegrityReport&);

  // Handles hash validation during SRI checks.
  static bool CheckHashesImpl(const WTF::HashSet<IntegrityMetadataPair>&,
                              const SegmentedBuffer*,
                              const KURL&,
                              IntegrityReport&);

  // Handles signature-based matching during SRI checks
  static bool CheckSignaturesImpl(const WTF::HashSet<IntegrityMetadataPair>&,
                                  const KURL& resource_url,
                                  const String& raw_headers,
                                  IntegrityReport&);

  enum AlgorithmParseResult {
    kAlgorithmValid,
    kAlgorithmUnparsable,
    kAlgorithmUnknown
  };

  static IntegrityAlgorithm FindBestAlgorithm(
      const WTF::HashSet<IntegrityMetadataPair>&);

  typedef bool (*CheckFunction)(const IntegrityMetadata&,
                                const char*,
                                size_t,
                                const String&);

  static bool CheckSubresourceIntegrityDigest(const IntegrityMetadata&,
                                              const SegmentedBuffer* buffer);

  static AlgorithmParseResult ParseAttributeAlgorithm(const UChar*& begin,
                                                      const UChar* end,
                                                      IntegrityAlgorithm&);
  typedef std::pair<const char*, IntegrityAlgorithm> AlgorithmPrefixPair;
  static bool ParseDigest(const UChar*& begin,
                          const UChar* end,
                          String& digest);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_SUBRESOURCE_INTEGRITY_H_
