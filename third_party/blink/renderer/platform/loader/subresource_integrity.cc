// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
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
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
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
    const FeatureContext* feature_context,
    IntegrityReport& integrity_report,
    HashMap<HashAlgorithm, String>* computed_hashes) {
  // FetchResponseType::kError never arrives because it is a loading error.
  DCHECK_NE(resource.GetResponse().GetType(),
            network::mojom::FetchResponseType::kError);
  if (!resource.GetResponse().IsCorsSameOrigin()) {
    integrity_report.AddConsoleErrorMessage(StrCat(
        {"Subresource Integrity: The resource '", resource_url.ElidedString(),
         "' has an integrity attribute, but the resource "
         "requires the request to be CORS enabled to check "
         "the integrity, and it is not. The resource has been "
         "blocked because the integrity cannot be enforced."}));
    integrity_report.AddUseCount(
        WebFeature::kSRIElementIntegrityAttributeButIneligible);
    return false;
  }

  const ResourceResponse& response = resource.GetResponse();
  String raw_headers = response.HttpHeaderFields().GetAsRawString(
      response.HttpStatusCode(), response.HttpStatusText());
  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       raw_headers, feature_context,
                                       integrity_report, computed_hashes);
}

bool SubresourceIntegrity::CheckSubresourceIntegrity(
    const IntegrityMetadataSet& metadata_set,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    FetchResponseType response_type,
    const String& raw_headers,
    const FeatureContext* feature_context,
    IntegrityReport& integrity_report) {
  // We're only going to check the integrity of non-errors, and
  // non-opaque responses.
  if (response_type != FetchResponseType::kBasic &&
      response_type != FetchResponseType::kCors &&
      response_type != FetchResponseType::kDefault) {
    integrity_report.AddConsoleErrorMessage(StrCat(
        {"Subresource Integrity: The resource '", resource_url.ElidedString(),
         "' has an integrity attribute, but the response is not eligible for "
         "integrity validation."}));
    return false;
  }

  return CheckSubresourceIntegrityImpl(metadata_set, buffer, resource_url,
                                       raw_headers, feature_context,
                                       integrity_report, nullptr);
}

String IntegrityAlgorithmToString(IntegrityAlgorithm algorithm,
                                  const FeatureContext* feature_context) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return "SHA-256";
    case IntegrityAlgorithm::kSha384:
      return "SHA-384";
    case IntegrityAlgorithm::kSha512:
      return "SHA-512";
    case IntegrityAlgorithm::kEd25519:
      return "Ed25519";
  }
}

String IntegrityAlgorithmsForConsole(const FeatureContext* feature_context) {
  return "'sha256', 'sha384', 'sha512', or 'ed21159'";
}

String HashAlgorithmToString(HashAlgorithm algorithm) {
  switch (algorithm) {
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
  return StrCat({HashAlgorithmToString(algorithm), Base64Encode(digest)});
}

String SubresourceIntegrity::GetSubresourceIntegrityHash(
    const SegmentedBuffer* buffer,
    HashAlgorithm algorithm) {
  DigestValue digest;
  if (!ComputeDigest(algorithm, buffer, digest)) {
    return String();
  }
  return GetIntegrityStringFromDigest(digest, algorithm);
}

bool SubresourceIntegrity::CheckUnencodedDigests(
    const Vector<IntegrityMetadata>& digests,
    const SegmentedBuffer* buffer) {
  // Performs the "verify `unencoded-digest` assertions" algorithm defined in
  // https://wicg.github.io/signature-based-sri/#unencoded-digest-validation
  for (const IntegrityMetadata& digest : digests) {
    HashAlgorithm algorithm =
        IntegrityAlgorithmToHashAlgorithm(digest.algorithm);
    DCHECK(algorithm == blink::HashAlgorithm::kHashAlgorithmSha256 ||
           algorithm == blink::HashAlgorithm::kHashAlgorithmSha512);

    DigestValue computed_digest;
    if (!ComputeDigest(algorithm, buffer, computed_digest)) {
      return false;
    }

    // If any specified digest doesn't match the digest computed over |buffer|,
    // matching fails.
    DigestValue expected_digest(base::as_byte_span(digest.value));
    if (computed_digest != expected_digest) {
      return false;
    }
  }

  // If no digest failed to match (or if we didn't have any digests in the
  // first place), matching succeeded.
  return true;
}

bool SubresourceIntegrity::CheckSubresourceIntegrityImpl(
    const IntegrityMetadataSet& parsed_metadata,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    const String& raw_headers,
    const FeatureContext* feature_context,
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

  // 3.  Let |hash-match| be `true` if |hash-metadata| is empty, and `false`
  //     otherwise.
  //
  //     (We're doing this in a slightly different order, breaking the hashing
  //      work into the `CheckHashesImpl()` block below.)
  if (!CheckHashesImpl(parsed_metadata.hashes, buffer, resource_url,
                       feature_context, integrity_report, computed_hashes)) {
    return false;
  }

  //
  // 6.  Let |signature-match| be `true` if |signature-metadata| is empty, and
  //     `false` otherwise.
  //
  //      (Our implementation is ordered differently from the spec: we check
  //       signature validity and match against integrity expectations in the
  //       network stack, directly after receiving headers. This means we're
  //       already done with signature-based SRI checks for network requests at
  //       this point, but we still might need to perform checks for resources
  //       that didn't come from the network (e.g. resources which were cached
  //       on the basis of one request, but are now being used in a context with
  //       different integrity requirements).
  if (!CheckSignaturesImpl(parsed_metadata.public_keys, resource_url,
                           raw_headers, integrity_report)) {
    return false;
  }
  return true;
}

bool SubresourceIntegrity::CheckHashesImpl(
    const Vector<IntegrityMetadata>& hashes,
    const SegmentedBuffer* buffer,
    const KURL& resource_url,
    const FeatureContext* feature_context,
    IntegrityReport& integrity_report,
    HashMap<HashAlgorithm, String>* computed_hashes) {
  // This implements steps 3 and 5 of
  // https://wicg.github.io/signature-based-sri/#matching.

  if (hashes.empty()) {
    return true;
  }

  // 3.  Let |hash-metadata| be the result of executing [Get the strongest
  //     metadata from set] on |parsedMetadata|["hashes"].
  //
  //     (We're doing this in a slightly different order, breaking the hashing
  //      work into the `CheckHashesImpl()` block below.)
  IntegrityAlgorithm strongest_algorithm = FindBestAlgorithm(hashes);

  // 5.  Let |actualValue| be the result of [Apply algorithm to bytes] on
  //      `algorithm` and `bytes`.
  //
  // To implement this, we precalculate |buffer|'s digest using the strongest
  // hashing algorithm:
  blink::HashAlgorithm hash_algo =
      IntegrityAlgorithmToHashAlgorithm(strongest_algorithm);
  DigestValue actual_value;
  if (!ComputeDigest(hash_algo, buffer, actual_value)) {
    integrity_report.AddConsoleErrorMessage(StrCat(
        {"There was an error computing an integrity value for resource '",
         resource_url.ElidedString(), "'. The resource has been blocked."}));
    return false;
  }

  // Then we loop through the asserted hashes, ignoring any that don't use
  // the strongest algorithm asserted:
  for (const IntegrityMetadata& metadata : hashes) {
    if (metadata.algorithm != strongest_algorithm) {
      continue;
    }

    // And finally decode the metadata's digest for comparison.
    DigestValue expected_value;
    expected_value.AppendSpan(base::as_byte_span(metadata.value));

    // 5.4. If actualValue is a case-sensitive match for expectedValue, return
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
      StrCat({"Failed to find a valid digest in the 'integrity' attribute for "
              "resource '",
              resource_url.ElidedString(), "' with computed ",
              IntegrityAlgorithmToString(strongest_algorithm, feature_context),
              " integrity '", Base64Encode(actual_value),
              "'. The resource has been blocked."}));
  integrity_report.AddUseCount(
      WebFeature::kSRIElementWithNonMatchingIntegrityAttribute);
  return false;
}

bool SubresourceIntegrity::CheckSignaturesImpl(
    const Vector<IntegrityMetadata>& integrity_list,
    const KURL& resource_url,
    const String& raw_headers,
    IntegrityReport& integrity_report) {
  // This implements steps 6 and 8.3 of
  // https://wicg.github.io/signature-based-sri/#matching.
  //
  if (integrity_list.empty()) {
    return true;
  }

  //
  // 8.3. Let |result| be the result of [validating an integrity signature]
  //      over response using algorithm and public key.
  //
  //      (Our implementation is ordered differently from the spec: we check
  //       signature validity and match against integrity expectations in the
  //       network stack, directly after receiving headers. This means we're
  //       already done with server-initiated SRI checks at this point.
  //
  //       This means we'll only get to this point in cases where
  //       the signature is both valid for SRI, and verifies as internally
  //       consistent (e.g. the public key in the `keyid` field can be used
  //       to validate the signature base.
  //
  //       With this in mind, all we need to do here is verify that at least
  //       one of the key digests in `parsed_metadata.signatures` matches at
  //       least one of the signatures we parsed from |raw_headers|.)
  Vector<network::mojom::blink::SRIMessageSignaturePtr> signatures =
      std::move(ParseSRIMessageSignaturesFromHeaders(raw_headers)->signatures);

  // This would be caught below, but we'll exit early for unsigned resources
  // so we can provide a better error message in the console.
  if (signatures.empty() && !integrity_list.empty()) {
    integrity_report.AddConsoleErrorMessage(
        StrCat({"Subresource Integrity: The resource at `",
                resource_url.ElidedString(),
                "` was not signed, but integrity "
                "checks are required. The resource has been blocked."}));
    return false;
  }

  for (const IntegrityMetadata& metadata : integrity_list) {
    for (const auto& signature : signatures) {
      if (signature->keyid &&
          (signature->keyid->size() == metadata.value.size() &&
           std::equal(metadata.value.begin(), metadata.value.end(),
                      signature->keyid->begin()))) {
        return true;
      }
    }
  }

  integrity_report.AddConsoleErrorMessage(StrCat(
      {"Subresource Integrity: The resource at `", resource_url.ElidedString(),
       "` was not signed in a way that "
       "matched the required integrity checks."}));
  return false;
}

IntegrityAlgorithm SubresourceIntegrity::FindBestAlgorithm(
    const Vector<IntegrityMetadata>& metadata_list) {
  // Find the "strongest" algorithm in the set. (This relies on
  // IntegrityAlgorithm declaration order matching the "strongest" order, so
  // make the compiler check this assumption first.)
  static_assert(IntegrityAlgorithm::kSha256 < IntegrityAlgorithm::kSha384 &&
                    IntegrityAlgorithm::kSha384 < IntegrityAlgorithm::kSha512,
                "IntegrityAlgorithm enum order should match the priority "
                "of the integrity algorithms.");

  // metadata_set is non-empty, so we are guaranteed to always have a result.
  DCHECK(!metadata_list.empty());
  return std::max_element(
             metadata_list.begin(), metadata_list.end(),
             [](const IntegrityMetadata& a, const IntegrityMetadata& b) {
               return a.algorithm < b.algorithm;
             })
      ->algorithm;
}

SubresourceIntegrity::AlgorithmParseResult
SubresourceIntegrity::ParseAttributeAlgorithm(
    std::string_view token,

    const FeatureContext* feature_context,
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
    const String& attribute,
    IntegrityMetadataSet& metadata_set,
    const FeatureContext* feature_context) {
  return ParseIntegrityAttribute(attribute, metadata_set, feature_context,
                                 nullptr);
}

void SubresourceIntegrity::ParseIntegrityAttribute(
    const String& attribute,
    IntegrityMetadataSet& metadata_set,
    const FeatureContext* feature_context,
    IntegrityReport* integrity_report) {
  // We expect a "clean" metadata_set, since metadata_set should only be filled
  // once.
  DCHECK(metadata_set.empty());

  StringUtf8Adaptor string_adapter(attribute);
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
        ParseAttributeAlgorithm(token, feature_context, algorithm);
    if (!parse_result.has_value()) {
      // Unknown hash algorithms are treated as if they're not present, and
      // thus are not marked as an error, they're just skipped.
      if (integrity_report) {
        switch (parse_result.error()) {
          case kAlgorithmUnknown:
            integrity_report->AddConsoleErrorMessage(
                StrCat({"Error parsing 'integrity' attribute ('", attribute,
                        "'). The specified hash algorithm must be one of ",
                        IntegrityAlgorithmsForConsole(feature_context), "."}));
            integrity_report->AddUseCount(
                WebFeature::kSRIElementWithUnparsableIntegrityAttribute);
            break;
          case kAlgorithmUnparsable:
            integrity_report->AddConsoleErrorMessage(
                StrCat({"Error parsing 'integrity' attribute ('", attribute,
                        "'). The hash algorithm must be one of ",
                        IntegrityAlgorithmsForConsole(feature_context),
                        ", followed by a '-' character."}));
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
            StrCat({"Error parsing 'integrity' attribute ('", attribute,
                    "'). The digest must be a valid, base64-encoded value."}));
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
            StrCat({"Ignoring unrecognized 'integrity' attribute option '",
                    String(maybe_options), "'."}));
      }
    }

    Vector<uint8_t> decoded;
    Base64Decode(digest, decoded);
    IntegrityMetadata integrity_metadata(algorithm, decoded);
    if (integrity_report) {
      if (IsHashingAlgorithm(algorithm)) {
        integrity_report->AddUseCount(WebFeature::kSRIHashAssertion);
      } else {
        integrity_report->AddUseCount(WebFeature::kSRIPublicKeyAssertion);
      }
    }
    metadata_set.Insert(std::move(integrity_metadata));
  }
}

bool SubresourceIntegrity::VerifyInlineIntegrity(
    const String& integrity_attr,
    const String& signature_attr,
    const String& source_code,
    const FeatureContext* feature_context) {
  if (!RuntimeEnabledFeatures::SignatureBasedInlineIntegrityEnabled()) {
    return true;
  }

  // Parse `integrity`:
  IntegrityMetadataSet integrity_metadata;
  if (!integrity_attr.empty()) {
    IntegrityReport integrity_report;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attr, integrity_metadata, feature_context, &integrity_report);
    // TODO(391907163): Log errors from |integrity_report|.
  }

  // Loop through asserted signatures, attempting to verify any of them using
  // any of the known keys. If any key can verify any signature, return true.
  int semantically_valid_signatures = 0;

  StringUtf8Adaptor sig_adaptor(signature_attr);
  StringUtf8Adaptor source_adaptor(source_code);
  for (std::string_view piece : base::SplitStringPiece(
           sig_adaptor.AsStringView(), base::kWhitespaceASCII,
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    auto [algorithm, base64_signature] =
        base::SplitStringOnce(piece, '-')
            .value_or(std::make_pair(std::string_view(), std::string_view()));
    if (algorithm != "ed25519") {
      // TODO(391907163): Log a warning for unknown (and therefore ignored)
      // signature algorithms.
      continue;
    }

    Vector<uint8_t> decoded_signature;
    if (!Base64Decode(StringView(base::as_byte_span(base64_signature)),
                      decoded_signature) ||
        decoded_signature.size() != 64u) {
      // TODO(391907163): Log an error for invalid signature digests.
      continue;
    }
    semantically_valid_signatures++;

    for (const auto& key : integrity_metadata.public_keys) {
      if (key.value.size() != 32u) {
        // TODO(391907163): Log an error for invalid public key digests.
        continue;
      }
      if (ED25519_verify(
              reinterpret_cast<const uint8_t*>(source_adaptor.data()),
              source_adaptor.size(), decoded_signature.data(),
              key.value.data())) {
        return true;
      }
    }
  }

  // If there were no signatures matching the grammar, then return true (as
  // we treat that case as not having any meaningful assertion). Otherwise,
  // no asserted signature could be validated above, so fail verification.
  //
  // TODO(391907163): Log an error for non-verifiable signatures.
  return semantically_valid_signatures == 0;
}

}  // namespace blink
