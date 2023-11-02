// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class SignedExchangeDevToolsProxy;

// signed_exchange_prologue namespace contains parsers for the first bytes of
// the "application/signed-exchange" format, preceding the cbor-encoded
// response header.
namespace signed_exchange_prologue {

// Parse 2-byte encoded length of the variable-length field in the signed
// exchange. Note: |input| must be pointing to a valid memory address that has
// at least 2 bytes.
CONTENT_EXPORT size_t Parse2BytesEncodedLength(base::span<const uint8_t> input);

// Parse 3-byte encoded length of the variable-length field in the signed
// exchange. Note: |input| must be pointing to a valid memory address that has
// at least 3 bytes.
CONTENT_EXPORT size_t Parse3BytesEncodedLength(base::span<const uint8_t> input);

// BeforeFallbackUrl holds the decoded data from the first
// |BeforeFallbackUrl::kEncodedSizeInBytes| bytes of the
// "application/signed-exchange" format.
class CONTENT_EXPORT BeforeFallbackUrl {
 public:
  // Size of the BeforeFallbackUrl part of "application/signed-exchange"
  // prologue.
  static const size_t kEncodedSizeInBytes;

  BeforeFallbackUrl() = default;
  BeforeFallbackUrl(bool is_valid, size_t fallback_url_length)
      : is_valid_(is_valid), fallback_url_length_(fallback_url_length) {}
  BeforeFallbackUrl(const BeforeFallbackUrl&) = default;
  ~BeforeFallbackUrl() = default;

  // Parses the first |kEncodedSizeInBytes| bytes of the
  // "application/signed-exchange" format.
  // |input| must be a valid span with length of |kEncodedSizeInBytes|.
  // If success, returns a |is_valid()| result.
  // Otherwise, returns a |!is_valid()| result and report the error to
  // |devtools_proxy|.
  static BeforeFallbackUrl Parse(base::span<const uint8_t> input,
                                 SignedExchangeDevToolsProxy* devtools_proxy);

  size_t ComputeFallbackUrlAndAfterLength() const;

  // |is_valid()| returns false if magic string was invalid.
  bool is_valid() const { return is_valid_; }

  size_t fallback_url_length() const { return fallback_url_length_; }

 private:
  bool is_valid_ = false;

  // Corresponds to `fallbackUrlLength` in the spec text.
  // Encoded length of the Signature header field's value.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  size_t fallback_url_length_ = 0;
};

class CONTENT_EXPORT FallbackUrlAndAfter {
 public:
  FallbackUrlAndAfter() = default;
  FallbackUrlAndAfter(const FallbackUrlAndAfter&) = default;
  ~FallbackUrlAndAfter() = default;

  // Parses the bytes of the "application/signed-exchange" format,
  // proceeding the BeforeFallbackUrl bytes.
  // |input| must be a valid span with length of
  // |before_fallback_url.ComputeFallbackUrlAndAfterLength()|.
  // If success, returns a |is_valid()| result.
  // Otherwise, returns a |!is_valid()| result and report the error to
  // |devtools_proxy|.
  static FallbackUrlAndAfter Parse(base::span<const uint8_t> input,
                                   const BeforeFallbackUrl& before_fallback_url,
                                   SignedExchangeDevToolsProxy* devtools_proxy);

  bool is_valid() const { return is_valid_; }

  // Note: fallback_url() may still be called even if |!is_valid()|,
  //       for trigering fallback redirect.
  const signed_exchange_utils::URLWithRawString& fallback_url() const {
    return fallback_url_;
  }

  size_t signature_header_field_length() const;
  size_t cbor_header_length() const;

  size_t ComputeFollowingSectionsLength() const;

 private:
  static FallbackUrlAndAfter ParseFailedButFallbackUrlAvailable(
      const signed_exchange_utils::URLWithRawString& fallback_url);

  FallbackUrlAndAfter(
      bool is_valid,
      const signed_exchange_utils::URLWithRawString& fallback_url,
      size_t signature_header_field_length,
      size_t cbor_header_length)
      : is_valid_(is_valid),
        fallback_url_(fallback_url),
        signature_header_field_length_(signature_header_field_length),
        cbor_header_length_(cbor_header_length) {}

  bool is_valid_ = false;

  // Corresponds to `fallbackUrl` in the spec text.
  // The URL to redirect navigation to when the signed exchange processing steps
  // has failed.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  signed_exchange_utils::URLWithRawString fallback_url_;

  // Corresponds to `sigLength` in the spec text.
  // Encoded length of the Signature header field's value.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  size_t signature_header_field_length_ = 0;

  // Corresponds to `headerLength` in the spec text.
  // Length of the CBOR representation of the request and response headers.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#application-signed-exchange
  size_t cbor_header_length_ = 0;
};

}  // namespace signed_exchange_prologue

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PROLOGUE_H_
