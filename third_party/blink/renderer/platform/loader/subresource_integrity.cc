// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// FIXME: This should probably use common functions with ContentSecurityPolicy.
static bool IsIntegrityCharacter(UChar c) {
  // Check if it's a base64 encoded value. We're pretty loose here, as there's
  // not much risk in it, and it'll make it simpler for developers.
  return IsASCIIAlphanumeric(c) || c == '_' || c == '-' || c == '+' ||
         c == '/' || c == '=';
}

static bool IsValueCharacter(UChar c) {
  // VCHAR per https://tools.ietf.org/html/rfc5234#appendix-B.1
  return c >= 0x21 && c <= 0x7e;
}

static bool DigestsEqual(const DigestValue& digest1,
                         const DigestValue& digest2) {
  return digest1 == digest2;
}

void SubresourceIntegrity::ReportInfo::AddUseCount(UseCounterFeature feature) {
  use_counts_.push_back(feature);
}

void SubresourceIntegrity::ReportInfo::AddConsoleErrorMessage(
    const String& message) {
  console_error_messages_.push_back(message);
}

void SubresourceIntegrity::ReportInfo::Clear() {
  use_counts_.clear();
  console_error_messages_.clear();
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    const Resource& resource,
    ReportInfo& report_info) {
  // FetchResponseType::kError never arrives because it is a loading error.
  DCHECK_NE(resource.GetResponse().GetType(),
            network::mojom::FetchResponseType::kError);
  if (!resource.GetResponse().IsCorsSameOrigin()) {
    report_info.AddConsoleErrorMessage(
        "Subresource Integrity: The resource '" + resource_url.ElidedString() +
        "' has an integrity attribute, but the resource "
        "requires the request to be CORS enabled to check "
        "the integrity, and it is not. The resource has been "
        "blocked because the integrity cannot be enforced.");
    report_info.AddUseCount(ReportInfo::UseCounterFeature::
                                kSRIElementIntegrityAttributeButIneligible);
    return false;
  }

  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       report_info);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const String& integrity_metadata,
    IntegrityFeatures features,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    ReportInfo& report_info) {
  if (integrity_metadata.empty())
    return true;

  IntegrityMetadataSet metadata_set;
  ParseIntegrityAttribute(integrity_metadata, features, metadata_set,
                          &report_info);
  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       report_info);
}

String IntegrityAlgorithmToString(IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return "SHA-256";
    case IntegrityAlgorithm::kSha384:
      return "SHA-384";
    case IntegrityAlgorithm::kSha512:
      return "SHA-512";
  }
  NOTREACHED_IN_MIGRATION();
}

blink::HashAlgorithm IntegrityAlgorithmToHashAlgorithm(
    IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return kHashAlgorithmSha256;
    case IntegrityAlgorithm::kSha384:
      return kHashAlgorithmSha384;
    case IntegrityAlgorithm::kSha512:
      return kHashAlgorithmSha512;
  }
  NOTREACHED_IN_MIGRATION();
}

bool SubresourceIntegrity::CheckSubresourceIntegrityImpl(
    const IntegrityMetadataSet& metadata_set,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    ReportInfo& report_info) {
  if (!metadata_set.size())
    return true;

  // Check any of the "strongest" integrity constraints.
  IntegrityAlgorithm max_algorithm = FindBestAlgorithm(metadata_set);
  for (const IntegrityMetadata& metadata : metadata_set) {
    if (metadata.Algorithm() == max_algorithm &&
        CheckSubresourceIntegrityDigest(metadata, buffer)) {
      report_info.AddUseCount(ReportInfo::UseCounterFeature::
                                  kSRIElementWithMatchingIntegrityAttribute);
      return true;
    }
  }

  // If we arrive here, none of the "strongest" constaints have validated
  // the data we received. Report this fact.
  DigestValue digest;
  if (ComputeDigest(IntegrityAlgorithmToHashAlgorithm(max_algorithm), buffer,
                    digest)) {
    // This message exposes the digest of the resource to the console.
    // Because this is only to the console, that's okay for now, but we
    // need to be very careful not to expose this in exceptions or
    // JavaScript, otherwise it risks exposing information about the
    // resource cross-origin.
    report_info.AddConsoleErrorMessage(
        "Failed to find a valid digest in the 'integrity' attribute for "
        "resource '" +
        resource_url.ElidedString() + "' with computed " +
        IntegrityAlgorithmToString(max_algorithm) + " integrity '" +
        Base64Encode(digest) + "'. The resource has been blocked.");
  } else {
    report_info.AddConsoleErrorMessage(
        "There was an error computing an integrity value for resource '" +
        resource_url.ElidedString() + "'. The resource has been blocked.");
  }
  report_info.AddUseCount(ReportInfo::UseCounterFeature::
                              kSRIElementWithNonMatchingIntegrityAttribute);
  return false;
}

IntegrityAlgorithm SubresourceIntegrity::FindBestAlgorithm(
    const IntegrityMetadataSet& metadata_set) {
  // Find the "strongest" algorithm in the set. (This relies on
  // IntegrityAlgorithm declaration order matching the "strongest" order, so
  // make the compiler check this assumption first.)
  static_assert(IntegrityAlgorithm::kSha256 < IntegrityAlgorithm::kSha384 &&
                    IntegrityAlgorithm::kSha384 < IntegrityAlgorithm::kSha512,
                "IntegrityAlgorithm enum order should match the priority "
                "of the integrity algorithms.");

  // metadata_set is non-empty, so we are guaranteed to always have a result.
  // This is effectively an implementation of std::max_element (C++17).
  DCHECK(!metadata_set.empty());
  auto iter = metadata_set.begin();
  IntegrityAlgorithm max_algorithm = iter->second;
  ++iter;
  for (; iter != metadata_set.end(); ++iter) {
    max_algorithm = std::max(iter->second, max_algorithm);
  }
  return max_algorithm;
}

bool SubresourceIntegrity::CheckSubresourceIntegrityDigest(
    const IntegrityMetadata& metadata,
    const SegmentedBuffer* buffer) {
  blink::HashAlgorithm hash_algo =
      IntegrityAlgorithmToHashAlgorithm(metadata.Algorithm());

  DigestValue digest;
  if (!ComputeDigest(hash_algo, buffer, digest)) {
    return false;
  }

  Vector<char> hash_vector;
  Base64Decode(metadata.Digest(), hash_vector);
  DigestValue converted_hash_vector;
  converted_hash_vector.AppendSpan(base::as_byte_span(hash_vector));
  return DigestsEqual(digest, converted_hash_vector);
}

SubresourceIntegrity::AlgorithmParseResult
SubresourceIntegrity::ParseAttributeAlgorithm(const UChar*& begin,
                                              const UChar* end,
                                              IntegrityFeatures features,
                                              IntegrityAlgorithm& algorithm) {
  static const AlgorithmPrefixPair kPrefixes[] = {
      {"sha256", IntegrityAlgorithm::kSha256},
      {"sha-256", IntegrityAlgorithm::kSha256},
      {"sha384", IntegrityAlgorithm::kSha384},
      {"sha-384", IntegrityAlgorithm::kSha384},
      {"sha512", IntegrityAlgorithm::kSha512},
      {"sha-512", IntegrityAlgorithm::kSha512}};

  // The last algorithm prefix is the ed25519 signature algorithm, which should
  // only be enabled if kSignatures is requested. We'll implement this by
  // adjusting the last_prefix index into the array.
  size_t last_prefix = std::size(kPrefixes);
  if (features != IntegrityFeatures::kSignatures)
    last_prefix--;

  return ParseAlgorithmPrefix(begin, end, kPrefixes, last_prefix, algorithm);
}

SubresourceIntegrity::AlgorithmParseResult
SubresourceIntegrity::ParseAlgorithmPrefix(
    const UChar*& string_position,
    const UChar* string_end,
    const AlgorithmPrefixPair* prefix_table,
    size_t prefix_table_size,
    IntegrityAlgorithm& algorithm) {
  for (size_t i = 0; i < prefix_table_size; i++) {
    const UChar* pos = string_position;
    if (SkipToken<UChar>(pos, string_end, prefix_table[i].first) &&
        SkipExactly<UChar>(pos, string_end, '-')) {
      string_position = pos;
      algorithm = prefix_table[i].second;
      return kAlgorithmValid;
    }
  }

  const UChar* dash_position = string_position;
  SkipUntil<UChar>(dash_position, string_end, '-');
  return dash_position < string_end ? kAlgorithmUnknown : kAlgorithmUnparsable;
}

// Before:
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//             ^     ^                        ^               ^
//      position   end                 position             end
//
// After (if successful: if the method returns false, we make no promises and
// the caller should exit early):
//
// [algorithm]-[hash]      OR     [algorithm]-[hash]?[options]
//                   ^                              ^         ^
//        position/end                       position       end
bool SubresourceIntegrity::ParseDigest(const UChar*& position,
                                       const UChar* end,
                                       String& digest) {
  const UChar* begin = position;
  SkipWhile<UChar, IsIntegrityCharacter>(position, end);
  if (position == begin || (position != end && *position != '?')) {
    digest = g_empty_string;
    return false;
  }

  // We accept base64url encoding, but normalize to "normal" base64 internally:
  digest = NormalizeToBase64(
      String(begin, static_cast<wtf_size_t>(position - begin)));
  return true;
}

void SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityFeatures features,
    IntegrityMetadataSet& metadata_set) {
  return ParseIntegrityAttribute(attribute, features, metadata_set, nullptr);
}

void SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityFeatures features,
    IntegrityMetadataSet& metadata_set,
    ReportInfo* report_info) {
  // We expect a "clean" metadata_set, since metadata_set should only be filled
  // once.
  DCHECK(metadata_set.empty());

  Vector<UChar> characters;
  attribute.StripWhiteSpace().AppendTo(characters);
  const UChar* position = characters.data();
  const UChar* end = characters.data() + characters.size();
  const UChar* current_integrity_end;

  // The integrity attribute takes the form:
  //    *WSP hash-with-options *( 1*WSP hash-with-options ) *WSP / *WSP
  // To parse this, break on whitespace, parsing each algorithm/digest/option
  // in order.
  while (position < end) {
    WTF::String digest;
    IntegrityAlgorithm algorithm;

    SkipWhile<UChar, IsASCIISpace>(position, end);
    current_integrity_end = position;
    SkipUntil<UChar, IsASCIISpace>(current_integrity_end, end);

    // Algorithm parsing errors are non-fatal (the subresource should
    // still be loaded) because strong hash algorithms should be used
    // without fear of breaking older user agents that don't support
    // them.
    AlgorithmParseResult parse_result = ParseAttributeAlgorithm(
        position, current_integrity_end, features, algorithm);
    if (parse_result == kAlgorithmUnknown) {
      // Unknown hash algorithms are treated as if they're not present,
      // and thus are not marked as an error, they're just skipped.
      SkipUntil<UChar, IsASCIISpace>(position, end);
      if (report_info) {
        report_info->AddConsoleErrorMessage(
            "Error parsing 'integrity' attribute ('" + attribute +
            "'). The specified hash algorithm must be one of "
            "'sha256', 'sha384', or 'sha512'.");
        report_info->AddUseCount(
            ReportInfo::UseCounterFeature::
                kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    if (parse_result == kAlgorithmUnparsable) {
      SkipUntil<UChar, IsASCIISpace>(position, end);
      if (report_info) {
        report_info->AddConsoleErrorMessage(
            "Error parsing 'integrity' attribute ('" + attribute +
            "'). The hash algorithm must be one of 'sha256', "
            "'sha384', or 'sha512', followed by a '-' "
            "character.");
        report_info->AddUseCount(
            ReportInfo::UseCounterFeature::
                kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    DCHECK_EQ(parse_result, kAlgorithmValid);

    if (!ParseDigest(position, current_integrity_end, digest)) {
      SkipUntil<UChar, IsASCIISpace>(position, end);
      if (report_info) {
        report_info->AddConsoleErrorMessage(
            "Error parsing 'integrity' attribute ('" + attribute +
            "'). The digest must be a valid, base64-encoded value.");
        report_info->AddUseCount(
            ReportInfo::UseCounterFeature::
                kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    // The spec defines a space in the syntax for options, separated by a
    // '?' character followed by unbounded VCHARs, but no actual options
    // have been defined yet. Thus, for forward compatibility, ignore any
    // options specified.
    if (SkipExactly<UChar>(position, end, '?')) {
      const UChar* begin = position;
      SkipWhile<UChar, IsValueCharacter>(position, end);
      if (begin != position && report_info) {
        report_info->AddConsoleErrorMessage(
            "Ignoring unrecogized 'integrity' attribute option '" +
            String(begin, static_cast<wtf_size_t>(position - begin)) + "'.");
      }
    }

    IntegrityMetadata integrity_metadata(digest, algorithm);
    metadata_set.insert(integrity_metadata.ToPair());
  }
}

}  // namespace blink
