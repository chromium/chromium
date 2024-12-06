// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include "base/strings/string_split.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/sri_message_signature.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/integrity_report.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
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

static bool IsHashingAlgorithm(IntegrityAlgorithm alg) {
  switch (alg) {
    case IntegrityAlgorithm::kSha256:
    case IntegrityAlgorithm::kSha384:
    case IntegrityAlgorithm::kSha512:
      return true;

    case IntegrityAlgorithm::kEd25519:
      return false;
  }
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    const Resource& resource,
    IntegrityReport& integrity_report,
    HashMap<HashAlgorithm, String>* computed_hashes) {
  // FetchResponseType::kError never arrives because it is a loading error.
  DCHECK_NE(resource.GetResponse().GetType(),
            network::mojom::FetchResponseType::kError);
  if (!resource.GetResponse().IsCorsSameOrigin()) {
    integrity_report.AddConsoleErrorMessage(
        "Subresource Integrity: The resource '" + resource_url.ElidedString() +
        "' has an integrity attribute, but the resource "
        "requires the request to be CORS enabled to check "
        "the integrity, and it is not. The resource has been "
        "blocked because the integrity cannot be enforced.");
    integrity_report.AddUseCount(
        WebFeature::kSRIElementIntegrityAttributeButIneligible);
    return false;
  }

  const ResourceResponse& response = resource.GetResponse();
  String raw_headers = response.HttpHeaderFields().GetAsRawString(
      response.HttpStatusCode(), response.HttpStatusText());
  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       raw_headers, integrity_report,
                                       computed_hashes);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    FetchResponseType response_type,
    const String& raw_headers,
    IntegrityReport& integrity_report) {
  // We're only going to check the integrity of non-errors, and
  // non-opaque responses.
  if (response_type != FetchResponseType::kBasic &&
      response_type != FetchResponseType::kCors &&
      response_type != FetchResponseType::kDefault) {
    integrity_report.AddConsoleErrorMessage(
        "Subresource Integrity: The resource '" + resource_url.ElidedString() +
        "' has an integrity attribute, but the response is not eligible for "
        "integrity validation.");
    return false;
  }

  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       raw_headers, integrity_report, nullptr);
}

String IntegrityAlgorithmToString(IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return "SHA-256";
    case IntegrityAlgorithm::kSha384:
      return "SHA-384";
    case IntegrityAlgorithm::kSha512:
      return "SHA-512";
    case IntegrityAlgorithm::kEd25519:
      DCHECK(RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled());
      return "Ed25519";
  }
}

String IntegrityAlgorithmsForConsole() {
  return RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled()
             ? "'sha256', 'sha384', 'sha512', or 'ed21159'"
             : "'sha256', 'sha384', or 'sha512'";
}

String HashAlgorithmToString(HashAlgorithm algorithm) {
  switch (algorithm) {
    case kHashAlgorithmSha1:
      NOTREACHED();
    case kHashAlgorithmSha256:
      return "sha256-";
    case kHashAlgorithmSha384:
      return "sha384-";
    case kHashAlgorithmSha512:
      return "sha512-";
  }
}

HashAlgorithm SubresourceIntegrity::IntegrityAlgorithmToHashAlgorithm(
    IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return kHashAlgorithmSha256;
    case IntegrityAlgorithm::kSha384:
      return kHashAlgorithmSha384;
    case IntegrityAlgorithm::kSha512:
      return kHashAlgorithmSha512;

    // We'll handle signature algorithms in a different flow, and shouldn't call
    // into this function at all for any non-hashing algorithms.
    case IntegrityAlgorithm::kEd25519:
      NOTREACHED();
  }
}

String GetIntegrityStringFromDigest(const DigestValue& digest,
                                    HashAlgorithm algorithm) {
  StringBuilder reported_hash;
  reported_hash.Append(HashAlgorithmToString(algorithm));
  reported_hash.Append(Base64Encode(digest));
  return reported_hash.ReleaseString();
}

std::optional<String> SubresourceIntegrity::GetSubresourceIntegrityHash(
    const SegmentedBuffer* buffer,
    HashAlgorithm algorithm) {
  DigestValue digest;
  if (!ComputeDigest(algorithm, buffer, digest)) {
    return std::nullopt;
  }
  return GetIntegrityStringFromDigest(digest, algorithm);
}

bool SubresourceIntegrity::CheckSubresourceIntegrityImpl(
    const IntegrityMetadataSet& parsed_metadata,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    const String& raw_headers,
    IntegrityReport& integrity_report,
    HashMap<HashAlgorithm, String>* computed_hashes) {
  // Implements https://wicg.github.io/signature-based-sri/#matching.
  //
  // 1.  Let |parsedMetadata| be the result of executing [Parse Metadata] on
  //     |metadataList|.
  //
  //     (We're receiving |parsed_metadata| as input to this function.)
  //
  // 2.  If both |parsedMetadata|["hashes"] and |parsedMetadata|["signatures"]
  //     are empty, return true.
  if (parsed_metadata.empty()) {
    return true;
  }

  // 3.  Let |hash-metadata| be the result of executing [Get the strongest
  //     metadata from set] on |parsedMetadata|["hashes"].
  // 4.  Let |signature-metadata| be the result of executing [Get the strongest
  //     metadata from set] on |parsedMetadata|["signatures"].
  //
  //     (We're doing these in a slightly different order, breaking the hashing
  //      work into the `CheckHashesImpl()` block below, and likewise the
  //      signature work into `CheckSignaturesImp()`.

  //
  // Verify the hash-based integrity constraints:
  //
  if (!CheckHashesImpl(parsed_metadata.hashes, buffer, resource_url,
                       integrity_report, computed_hashes)) {
    return false;
  }

  //
  // And the signature-based constraints (iff the relevant runtime-enabled
  // feature is enabled).
  //
  if (RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled() &&
      !CheckSignaturesImpl(parsed_metadata.signatures, resource_url,
                           raw_headers, integrity_report)) {
    return false;
  }
  return true;
}

bool SubresourceIntegrity::CheckHashesImpl(
    const WTF::HashSet<IntegrityMetadataPair>& hashes,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    IntegrityReport& integrity_report,
    HashMap<HashAlgorithm, String>* computed_hashes) {
  // This implements steps 3, 5, and 7 of
  // https://wicg.github.io/signature-based-sri/#matching.

  // 5.  Let |hash-match| be `true` if |hash-metadata| is empty, and `false`
  //     otherwise.
  if (hashes.empty()) {
    return true;
  }

  // This is more or less step 3 (at least, it is in combination with the
  // checks in the loop below that ignore non-matching algorithms). We run it
  // after 5, as `FindBestAlgorithm` assumes that |hashes| is not empty.
  IntegrityAlgorithm strongest_algorithm = FindBestAlgorithm(hashes);

  // 7.3. Let |actualValue| be the result of [Apply algorithm to bytes] on
  //      `algorithm` and `bytes`.
  //
  // To implement this, we precalculate |buffer|'s digest using the strongest
  // hashing algorithm:
  blink::HashAlgorithm hash_algo =
      IntegrityAlgorithmToHashAlgorithm(strongest_algorithm);
  DigestValue actual_value;
  if (!ComputeDigest(hash_algo, buffer, actual_value)) {
    integrity_report.AddConsoleErrorMessage(
        "There was an error computing an integrity value for resource '" +
        resource_url.ElidedString() + "'. The resource has been blocked.");
    return false;
  }

  // Then we loop through the asserted hashes, ignoring any that don't use
  // the strongest algorithm asserted:
  for (const IntegrityMetadata& metadata : hashes) {
    if (metadata.Algorithm() != strongest_algorithm) {
      continue;
    }

    // And finally decode the metadata's digest for comparison.
    Vector<char> decoded_metadata;
    Base64Decode(metadata.Digest(), decoded_metadata);
    DigestValue expected_value;
    expected_value.AppendSpan(base::as_byte_span(decoded_metadata));

    // 7.4. If actualValue is a case-sensitive match for expectedValue, return
    // true set hash-match to true and break.
    if (actual_value == expected_value) {
      integrity_report.AddUseCount(
          WebFeature::kSRIElementWithMatchingIntegrityAttribute);
      if (computed_hashes) {
        computed_hashes->insert(
            hash_algo, GetIntegrityStringFromDigest(actual_value, hash_algo));
      }
      return true;
    }
  }

  // Record failure if no digest match was found:
  //
  // This message exposes the digest of the resource to the console.
  // Because this is only to the console, that's okay for now, but we
  // need to be very careful not to expose this in exceptions or
  // JavaScript, otherwise it risks exposing information about the
  // resource cross-origin.
  integrity_report.AddConsoleErrorMessage(
      "Failed to find a valid digest in the 'integrity' attribute for "
      "resource '" +
      resource_url.ElidedString() + "' with computed " +
      IntegrityAlgorithmToString(strongest_algorithm) + " integrity '" +
      Base64Encode(actual_value) + "'. The resource has been blocked.");
  integrity_report.AddUseCount(
      WebFeature::kSRIElementWithNonMatchingIntegrityAttribute);
  return false;
}

bool SubresourceIntegrity::CheckSignaturesImpl(
    const WTF::HashSet<IntegrityMetadataPair>& integrity_pairs,
    const KURL& resource_url,
    const String& raw_headers,
    IntegrityReport& integrity_report) {
  // This implements steps 6 and 8 of
  // https://wicg.github.io/signature-based-sri/#matching.
  //
  // (For the moment we're skipping step 4, as we only have one signature
  // type, so a list of the "strongest" obviously includes all of them.)
  //
  //
  // 6.  Let |signature-match| be `true` if |signature-metadata| is empty, and
  //     `false` otherwise.
  if (integrity_pairs.empty()) {
    return true;
  }

  //
  // 8.3. Let |result| be the result of [validating an integrity signature]
  //      over response using algorithm and public key.
  //
  //      (Our implementation is ordered differently from the spec: we check
  //       signature validity in the network stack, directly after receiving
  //       headers. This means we'll only get to this point in cases where
  //       the signature is both valid for SRI, and verifies as internally
  //       consistent (e.g. the public key in the `keyid` field can be used
  //       to validate the signature base.
  //
  //       With this in mind, all we need to do here is verify that at least
  //       one of the key digests in `parsed_metadata.signatures` matches at
  //       least one of the signatures we parsed from |raw_headers|.)
  Vector<network::mojom::blink::SRIMessageSignaturePtr> signatures =
      ParseSRIMessageSignaturesFromHeaders(raw_headers);

  // This would be caught below, but we'll exit early for unsigned resources
  // so we can provide a better error message in the console.
  if (signatures.empty() && !integrity_pairs.empty()) {
    integrity_report.AddConsoleErrorMessage(
        "Subresource Integrity: The resource at `" +
        resource_url.ElidedString() +
        "` was not signed, but integrity "
        "checks are required. The resource has been blocked.");
    return false;
  }

  for (const IntegrityMetadata& metadata : integrity_pairs) {
    String public_key = metadata.Digest();
    for (const auto& signature : signatures) {
      if (signature->keyid == public_key) {
        return true;
      }
    }
  }

  integrity_report.AddConsoleErrorMessage(
      "Subresource Integrity: The resource at `" + resource_url.ElidedString() +
      "` was not signed in a way that "
      "matched the required integrity checks.");
  return false;
}

IntegrityAlgorithm SubresourceIntegrity::FindBestAlgorithm(
    const WTF::HashSet<IntegrityMetadataPair>& metadata_pairs) {
  // Find the "strongest" algorithm in the set. (This relies on
  // IntegrityAlgorithm declaration order matching the "strongest" order, so
  // make the compiler check this assumption first.)
  static_assert(IntegrityAlgorithm::kSha256 < IntegrityAlgorithm::kSha384 &&
                    IntegrityAlgorithm::kSha384 < IntegrityAlgorithm::kSha512,
                "IntegrityAlgorithm enum order should match the priority "
                "of the integrity algorithms.");

  // metadata_set is non-empty, so we are guaranteed to always have a result.
  // This is effectively an implementation of std::max_element (C++17).
  DCHECK(!metadata_pairs.empty());
  auto iter = metadata_pairs.begin();
  IntegrityAlgorithm max_algorithm = iter->second;
  ++iter;
  for (; iter != metadata_pairs.end(); ++iter) {
    max_algorithm = std::max(iter->second, max_algorithm);
  }
  return max_algorithm;
}

SubresourceIntegrity::AlgorithmParseResult
SubresourceIntegrity::ParseAttributeAlgorithm(std::string_view token,
                                              IntegrityAlgorithm& algorithm) {
  static const AlgorithmPrefixPair kPrefixes[] = {
      {"sha256", IntegrityAlgorithm::kSha256},
      {"sha-256", IntegrityAlgorithm::kSha256},
      {"sha384", IntegrityAlgorithm::kSha384},
      {"sha-384", IntegrityAlgorithm::kSha384},
      {"sha512", IntegrityAlgorithm::kSha512},
      {"sha-512", IntegrityAlgorithm::kSha512},
      {"ed25519", IntegrityAlgorithm::kEd25519}};

  for (const auto& [prefix_cstr, algorithm_enum] : kPrefixes) {
    const std::string_view prefix(prefix_cstr);
    // Parse signature-based algorithm prefixes iff the runtime feature is
    // enabled.
    if (!RuntimeEnabledFeatures::SignatureBasedIntegrityEnabled() &&
        prefix == "ed25519") {
      continue;
    }
    if (token.starts_with(prefix) && token.size() > prefix.size() &&
        token[prefix.size()] == '-') {
      algorithm = algorithm_enum;
      return base::ok(prefix.size());
    }
  }

  const bool contains_dash = token.find('-') != std::string_view::npos;
  return contains_dash ? base::unexpected(kAlgorithmUnknown)
                       : base::unexpected(kAlgorithmUnparsable);
}

bool SubresourceIntegrity::ParseDigest(std::string_view maybe_digest,
                                       String& digest) {
  if (maybe_digest.empty() ||
      !std::ranges::all_of(maybe_digest, IsIntegrityCharacter)) {
    digest = g_empty_string;
    return false;
  }

  // We accept base64url encoding, but normalize to "normal" base64 internally:
  digest = NormalizeToBase64(String(maybe_digest));
  return true;
}

void SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityMetadataSet& metadata_set) {
  return ParseIntegrityAttribute(attribute, metadata_set, nullptr);
}

void SubresourceIntegrity::ParseIntegrityAttribute(
    const WTF::String& attribute,
    IntegrityMetadataSet& metadata_set,
    IntegrityReport* integrity_report) {
  // We expect a "clean" metadata_set, since metadata_set should only be filled
  // once.
  DCHECK(metadata_set.empty());

  StringUTF8Adaptor string_adapter(attribute);
  std::string_view characters = base::TrimWhitespaceASCII(
      base::as_string_view(string_adapter), base::TRIM_ALL);

  // The integrity attribute takes the form:
  //    *WSP hash-with-options *( 1*WSP hash-with-options ) *WSP / *WSP
  // To parse this, break on whitespace, parsing each algorithm/digest/option
  // in order.
  for (const auto& token : base::SplitStringPiece(
           characters, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    // Algorithm parsing errors are non-fatal (the subresource should
    // still be loaded) because strong hash algorithms should be used
    // without fear of breaking older user agents that don't support
    // them.
    IntegrityAlgorithm algorithm;
    AlgorithmParseResult parse_result =
        ParseAttributeAlgorithm(token, algorithm);
    if (!parse_result.has_value()) {
      // Unknown hash algorithms are treated as if they're not present, and
      // thus are not marked as an error, they're just skipped.
      if (integrity_report) {
        switch (parse_result.error()) {
          case kAlgorithmUnknown:
            integrity_report->AddConsoleErrorMessage(
                "Error parsing 'integrity' attribute ('" + attribute +
                "'). The specified hash algorithm must be one of " +
                IntegrityAlgorithmsForConsole() + ".");
            integrity_report->AddUseCount(
                WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
            break;
          case kAlgorithmUnparsable:
            integrity_report->AddConsoleErrorMessage(
                "Error parsing 'integrity' attribute ('" + attribute +
                "'). The hash algorithm must be one of " +
                IntegrityAlgorithmsForConsole() +
                ", followed by a '-' character.");
            integrity_report->AddUseCount(
                WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
            break;
        }
      }
      continue;
    }

    const size_t prefix_length = parse_result.value();
    auto rest = token.substr(prefix_length + 1);
    auto [maybe_digest, maybe_options] =
        base::SplitStringOnce(rest, '?').value_or(
            std::make_pair(rest, std::string_view()));

    String digest;
    if (!ParseDigest(maybe_digest, digest)) {
      if (integrity_report) {
        integrity_report->AddConsoleErrorMessage(
            "Error parsing 'integrity' attribute ('" + attribute +
            "'). The digest must be a valid, base64-encoded value.");
        integrity_report->AddUseCount(
            WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
      }
      continue;
    }

    // The spec defines a space in the syntax for options, separated by a
    // '?' character followed by unbounded VCHARs, but no actual options
    // have been defined yet. Thus, for forward compatibility, ignore any
    // options specified.
    if (integrity_report && !maybe_options.empty()) {
      if (std::ranges::all_of(maybe_options, IsValueCharacter)) {
        integrity_report->AddConsoleErrorMessage(
            "Ignoring unrecognized 'integrity' attribute option '" +
            String(maybe_options) + "'.");
      }
    }

    IntegrityMetadata integrity_metadata(digest, algorithm);
    if (IsHashingAlgorithm(algorithm)) {
      metadata_set.hashes.insert(integrity_metadata.ToPair());
    } else {
      metadata_set.signatures.insert(integrity_metadata.ToPair());
    }
  }
}

}  // namespace blink
