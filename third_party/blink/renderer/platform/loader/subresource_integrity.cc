// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include "base/stl_util.h"
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
#include "third_party/boringssl/src/include/openssl/curve25519.h"

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
  if (digest1.size() != digest2.size())
    return false;

  for (wtf_size_t i = 0; i < digest1.size(); i++) {
    if (digest1[i] != digest2[i])
      return false;
  }

  return true;
}

inline bool IsSpaceOrComma(UChar c) {
  return IsASCIISpace(c) || c == ',';
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
    const char* content,
    size_t size,
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

  return CheckSubresourceIntegrityImpl(
      metadata_set, content, size, resource_url,
      resource.GetResponse().HttpHeaderField("Integrity"), report_info);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const String& integrity_metadata,
    IntegrityFeatures features,
    const char* content,
    size_t size,
    const KURL& resource_url,
    ReportInfo& report_info) {
  if (integrity_metadata.IsEmpty())
    return true;

  IntegrityMetadataSet metadata_set;
  IntegrityParseResult integrity_parse_result = ParseIntegrityAttribute(
      integrity_metadata, features, metadata_set, &report_info);
  if (integrity_parse_result != kIntegrityParseValidResult)
    return true;
  // TODO(vogelheim): crbug.com/753349, figure out how deal with Ed25519
  //                  checking here.
  String integrity_header;
  return CheckSubresourceIntegrityImpl(
      metadata_set, content, size, resource_url, integrity_header, report_info);
}

bool SubresourceIntegrity::CheckSubresourceIntegrityImpl(
    const IntegrityMetadataSet& metadata_set,
    const char* content,
    size_t size,
    const KURL& resource_url,
    const String integrity_header,
    ReportInfo& report_info) {
  if (!metadata_set.size())
    return true;

  // Check any of the "strongest" integrity constraints.
  IntegrityAlgorithm max_algorithm = FindBestAlgorithm(metadata_set);
  CheckFunction checker = GetCheckFunctionForAlgorithm(max_algorithm);
  bool report_ed25519 = max_algorithm == IntegrityAlgorithm::kEd25519;
  if (report_ed25519) {
    report_info.AddUseCount(ReportInfo::UseCounterFeature::kSRISignatureCheck);
  }
  for (const IntegrityMetadata& metadata : metadata_set) {
    if (metadata.Algorithm() == max_algorithm &&
        (*checker)(metadata, content, size, integrity_header)) {
      report_info.AddUseCount(ReportInfo::UseCounterFeature::
                                  kSRIElementWithMatchingIntegrityAttribute);
      if (report_ed25519) {
        report_info.AddUseCount(
            ReportInfo::UseCounterFeature::kSRISignatureSuccess);
      }
      return true;
    }
  }

  // If we arrive here, none of the "strongest" constaints have validated
  // the data we received. Report this fact.
  DigestValue digest;
  if (ComputeDigest(kHashAlgorithmSha256, content, size, digest)) {
    // This message exposes the digest of the resource to the console.
    // Because this is only to the console, that's okay for now, but we
    // need to be very careful not to expose this in exceptions or
    // JavaScript, otherwise it risks exposing information about the
    // resource cross-origin.
    report_info.AddConsoleErrorMessage(
        "Failed to find a valid digest in the 'integrity' attribute for "
        "resource '" +
        resource_url.ElidedString() + "' with computed SHA-256 integrity '" +
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
                    IntegrityAlgorithm::kSha384 < IntegrityAlgorithm::kSha512 &&
                    IntegrityAlgorithm::kSha512 < IntegrityAlgorithm::kEd25519,
                "IntegrityAlgorithm enum order should match the priority "
                "of the integrity algorithms.");

  // metadata_set is non-empty, so we are guaranteed to always have a result.
  // This is effectively an implemenation of std::max_element (C++17).
  DCHECK(!metadata_set.IsEmpty());
  auto iter = metadata_set.begin();
  IntegrityAlgorithm max_algorithm = iter->second;
  ++iter;
  for (; iter != metadata_set.end(); ++iter) {
    max_algorithm = std::max(iter->second, max_algorithm);
  }
  return max_algorithm;
}

SubresourceIntegrity::CheckFunction
SubresourceIntegrity::GetCheckFunctionForAlgorithm(
    IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
    case IntegrityAlgorithm::kSha384:
    case IntegrityAlgorithm::kSha512:
      return SubresourceIntegrity::CheckSubresourceIntegrityDigest;
    case IntegrityAlgorithm::kEd25519:
      return SubresourceIntegrity::CheckSubresourceIntegritySignature;
  }
  NOTREACHED();
  return nullptr;
}

bool SubresourceIntegrity::CheckSubresourceIntegrityDigest(
    const IntegrityMetadata& metadata,
    const char* content,
    size_t size,
    const String& integrity_header) {
  blink::HashAlgorithm hash_algo = kHashAlgorithmSha256;
  switch (metadata.Algorithm()) {
    case IntegrityAlgorithm::kSha256:
      hash_algo = kHashAlgorithmSha256;
      break;
    case IntegrityAlgorithm::kSha384:
      hash_algo = kHashAlgorithmSha384;
      break;
    case IntegrityAlgorithm::kSha512:
      hash_algo = kHashAlgorithmSha512;
      break;
    case IntegrityAlgorithm::kEd25519:
      NOTREACHED();
      break;
  }

  DigestValue digest;
  if (!ComputeDigest(hash_algo, content, size, digest))
    return false;

  Vector<char> hash_vector;
  Base64Decode(metadata.Digest(), hash_vector);
  DigestValue converted_hash_vector;
  converted_hash_vector.Append(reinterpret_cast<uint8_t*>(hash_vector.data()),
                               hash_vector.size());
  return DigestsEqual(digest, converted_hash_vector);
}

bool SubresourceIntegrity::CheckSubresourceIntegritySignature(
    const IntegrityMetadata& metadata,
    const char* content,
    size_t size,
    const String& integrity_header) {
  DCHECK_EQ(IntegrityAlgorithm::kEd25519, metadata.Algorithm());

  Vector<char> pubkey;
  if (!Base64Decode(metadata.Digest(), pubkey) ||
      pubkey.size() != ED25519_PUBLIC_KEY_LEN)
    return false;

  // Parse the Integrity:-header containing the signature(s).
  Vector<UChar> integrity_header_chars;
  integrity_header.AppendTo(integrity_header_chars);
  const UChar* position = integrity_header_chars.begin();

  const UChar* const end_position = integrity_header_chars.end();
  while (position < end_position) {
    // We expect substrings of the form "ed25519-<BASE64>* ,".
    // We'll move all of our UChar* pointers up front (before any early exits
    // from the loop), since we should cleanly skip the next token in the
    // header in all cases, even if the current token doesn't validate.
    SkipWhile<UChar, IsSpaceOrComma>(position, end_position);
    IntegrityAlgorithm algorithm;
    bool found_ed25519 =
        kAlgorithmValid ==
            ParseIntegrityHeaderAlgorithm(position, end_position, algorithm) &&
        IntegrityAlgorithm::kEd25519 == algorithm;
    const UChar* digest_begin = position;
    SkipUntil<UChar, IsSpaceOrComma>(position, end_position);
    const UChar* const digest_end = position;

    // Now, algorithm contains the parsed algorithm specifier, the digest is
    // found at digest_begin..digest_end, and position sits before the next
    // token.

    if (!found_ed25519)
      continue;

    String signature_raw;
    if (!ParseDigest(digest_begin, digest_end, signature_raw))
      continue;

    Vector<char> signature;
    Base64Decode(signature_raw, signature);
    if (signature.size() != ED25519_SIGNATURE_LEN)
      continue;

    // BoringSSL/OpenSSL functions return 1 for success.
    if (1 ==
        ED25519_verify(reinterpret_cast<const uint8_t*>(content), size,
                       reinterpret_cast<const uint8_t*>(&*signature.begin()),
                       reinterpret_cast<const uint8_t*>(&*pubkey.begin()))) {
      return true;
    }
  }
  return false;
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
      {"sha-512", IntegrityAlgorithm::kSha512},
      {"ed25519", IntegrityAlgorithm::kEd25519}};

  // The last algorithm prefix is the ed25519 signature algorithm, which should
  // only be enabled if kSignatures is requested. We'll implement this by
  // adjusting the last_prefix index into the array.
  size_t last_prefix = base::size(kPrefixes);
  if (features != IntegrityFeatures::kSignatures)
    last_prefix--;

  return ParseAlgorithmPrefix(begin, end, kPrefixes, last_prefix, algorithm);
}

SubresourceIntegrity::AlgorithmParseResult
SubresourceIntegrity::ParseIntegrityHeaderAlgorithm(
    const UChar*& begin,
    const UChar* end,
    IntegrityAlgorithm& algorithm) {
  static const AlgorithmPrefixPair kPrefixes[] = {
      {"ed25519", IntegrityAlgorithm::kEd25519}};
  return ParseAlgorithmPrefix(begin, end, kPrefixes, base::size(kPrefixes),
                              algorithm);
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

SubresourceIntegrity::IntegrityParseResult
SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityFeatures features,
    IntegrityMetadataSet& metadata_set) {
  return ParseIntegrityAttribute(attribute, features, metadata_set, nullptr);
}

SubresourceIntegrity::IntegrityParseResult
SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityFeatures features,
    IntegrityMetadataSet& metadata_set,
    ReportInfo* report_info) {
  // We expect a "clean" metadata_set, since metadata_set should only be filled
  // once.
  DCHECK(metadata_set.IsEmpty());

  Vector<UChar> characters;
  attribute.StripWhiteSpace().AppendTo(characters);
  const UChar* position = characters.data();
  const UChar* end = characters.end();
  const UChar* current_integrity_end;

  bool error = false;

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
      error = true;
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
      error = true;
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
  if (metadata_set.size() == 0 && error)
    return kIntegrityParseNoValidResult;

  return kIntegrityParseValidResult;
}

}  // namespace blink
