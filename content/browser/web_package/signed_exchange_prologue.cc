// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_prologue.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_utils.h"

namespace content {

namespace {

constexpr char kSignedExchangeMagic[] = "sxg1-b3";

// size of `fallbackUrlLength` field in number of bytes.
constexpr size_t kFallbackUrlLengthFieldSizeInBytes = 2;
// size of `sigLength` field in number of bytes.
constexpr size_t kSigLengthFieldLengthInBytes = 3;
// size of `headerLength` field in number of bytes.
constexpr size_t kHeaderLengthFieldLengthInBytes = 3;

constexpr size_t kMaximumSignatureHeaderFieldLength = 16 * 1024;
constexpr size_t kMaximumCBORHeaderLength = 512 * 1024;

}  // namespace

namespace signed_exchange_prologue {

const size_t BeforeFallbackUrl::kEncodedSizeInBytes =
    sizeof(kSignedExchangeMagic) + kFallbackUrlLengthFieldSizeInBytes;

size_t Parse2BytesEncodedLength(base::span<const uint8_t> input) {
  DCHECK_EQ(input.size(), 2u);
  return static_cast<size_t>(input[0]) << 8 | static_cast<size_t>(input[1]);
}

size_t Parse3BytesEncodedLength(base::span<const uint8_t> input) {
  DCHECK_EQ(input.size(), 3u);
  return static_cast<size_t>(input[0]) << 16 |
         static_cast<size_t>(input[1]) << 8 | static_cast<size_t>(input[2]);
}

// static
BeforeFallbackUrl BeforeFallbackUrl::Parse(
    base::span<const uint8_t> input,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "signed_exchange_prologue::BeforeFallbackUrl::Parse");

  CHECK_EQ(input.size(), kEncodedSizeInBytes);

  const auto magic_string = input.first(sizeof(kSignedExchangeMagic));
  const auto encoded_fallback_url_length_field = input.subspan(
      sizeof(kSignedExchangeMagic), kFallbackUrlLengthFieldSizeInBytes);

  bool is_valid = true;
  if (memcmp(magic_string.data(), kSignedExchangeMagic,
             sizeof(kSignedExchangeMagic)) != 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Wrong magic string");
    is_valid = false;
  }

  size_t fallback_url_length =
      Parse2BytesEncodedLength(encoded_fallback_url_length_field);
  return BeforeFallbackUrl(is_valid, fallback_url_length);
}

size_t BeforeFallbackUrl::ComputeFallbackUrlAndAfterLength() const {
  return fallback_url_length_ + kSigLengthFieldLengthInBytes +
         kHeaderLengthFieldLengthInBytes;
}

// static
FallbackUrlAndAfter FallbackUrlAndAfter::ParseFailedButFallbackUrlAvailable(
    const signed_exchange_utils::URLWithRawString& fallback_url) {
  return FallbackUrlAndAfter(/*is_valid=*/false, fallback_url,
                             /*signature_header_field_length=*/0,
                             /*cbor_header_length=*/0);
}

// static
FallbackUrlAndAfter FallbackUrlAndAfter::Parse(
    base::span<const uint8_t> input,
    const BeforeFallbackUrl& before_fallback_url,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "signed_exchange_prologue::FallbackUrlAndAfter::Parse");

  if (input.size() < before_fallback_url.fallback_url_length()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "End of stream reached before reading the entire `fallbackUrl`.");
    return FallbackUrlAndAfter();
  }

  std::string_view fallback_url_str(reinterpret_cast<const char*>(input.data()),
                                    before_fallback_url.fallback_url_length());
  if (!base::IsStringUTF8(fallback_url_str)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "`fallbackUrl` is not a valid UTF-8 sequence.");
    return FallbackUrlAndAfter();
  }

  signed_exchange_utils::URLWithRawString fallback_url(fallback_url_str);
  if (!fallback_url.url.is_valid()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse `fallbackUrl`.");
    return FallbackUrlAndAfter();
  }
  if (!fallback_url.url.SchemeIs(url::kHttpsScheme)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "`fallbackUrl` in non-https scheme.");
    return FallbackUrlAndAfter();
  }
  if (fallback_url.url.has_ref()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "`fallbackUrl` can't have a fragment.");
    return FallbackUrlAndAfter();
  }

  // Note: For the code path after this comment, if parsing failed but
  //       the `fallbackUrl` parse had succeed, the return value can still be
  //       used for fallback redirect.

  if (!before_fallback_url.is_valid())
    return ParseFailedButFallbackUrlAvailable(fallback_url);

  if (input.size() < before_fallback_url.ComputeFallbackUrlAndAfterLength()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "End of stream reached before reading `sigLength` and `headerLength` "
        "fields.");
    return ParseFailedButFallbackUrlAvailable(fallback_url);
  }

  const auto encoded_signature_header_field_length = input.subspan(
      before_fallback_url.fallback_url_length(), kSigLengthFieldLengthInBytes);
  const auto encoded_cbor_header_length = input.subspan(
      before_fallback_url.fallback_url_length() + kSigLengthFieldLengthInBytes,
      kHeaderLengthFieldLengthInBytes);

  size_t signature_header_field_length =
      Parse3BytesEncodedLength(encoded_signature_header_field_length);
  size_t cbor_header_length =
      Parse3BytesEncodedLength(encoded_cbor_header_length);

  if (signature_header_field_length > kMaximumSignatureHeaderFieldLength) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Signature header field too long: %zu",
                           signature_header_field_length));
    return ParseFailedButFallbackUrlAvailable(fallback_url);
  }
  if (cbor_header_length > kMaximumCBORHeaderLength) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("CBOR header too long: %zu", cbor_header_length));
    return ParseFailedButFallbackUrlAvailable(fallback_url);
  }

  return FallbackUrlAndAfter(true, fallback_url, signature_header_field_length,
                             cbor_header_length);
}

size_t FallbackUrlAndAfter::signature_header_field_length() const {
  DCHECK(is_valid());
  return signature_header_field_length_;
}

size_t FallbackUrlAndAfter::cbor_header_length() const {
  DCHECK(is_valid());
  return cbor_header_length_;
}

size_t FallbackUrlAndAfter::ComputeFollowingSectionsLength() const {
  DCHECK(is_valid());
  return signature_header_field_length_ + cbor_header_length_;
}

}  // namespace signed_exchange_prologue

}  // namespace content
