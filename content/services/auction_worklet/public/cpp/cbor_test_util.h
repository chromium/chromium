// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CBOR_TEST_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CBOR_TEST_UTIL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "components/cbor/values.h"

namespace auction_worklet::test {

// Helper to convert a JSON string to a CBOR vector. CHECKs on error.
std::vector<uint8_t> ToCborVector(std::string_view json);

// Helper to convert a JSON string to a CBOR string. CHECKs on error.
std::string ToCborString(std::string_view json);

// Helper to convert a JSON string to a CBOR string, but designed to handle KVv2
// responses. Any dictionary entry with a key of "content" will have its value
// converted to a binary CBOR string. For trusted key-value V2 responses, each
// "content" string is a compression group, encoded independently of the main
// response. Does not compress content strings, so only works with the KVv2
// uncompressed compression scheme. This behavior is not applied recursively.
// CHECKs on error.
std::string ToKVv2ResponseCborString(std::string_view json);

// Takes response body string as a CBOR-encoded string, adds padding so that the
// body size will be a power of 2 when prefixed by OHTTP headers, and adds a
// framing header (compression format, length). This uses logic taken from
// TrustedSiganlsFetcher, so tests specifically of framing should not use this
// method.
std::string CreateKVv2RequestBody(std::string_view cbor_request_body);

// Takes response body string as a CBOR-encoded string, adds a framing header
// (compression format, length) and, optionally, padding, and returns it as a
// string. If `advertised_cbor_length` is std::nullopt, uses the length of the
// input string. `padding_length` 0's are appended to the end of the response
// body. Compression scheme is taken as a raw byte, to allow testing of values
// not representable by auction_worklet::mojom::TrustedSignalsCompressionScheme,
// and will write to the entire leading 8-bits, instead of only the low-order 2
// bits that are actually the compression scheme. The default value of 0
// corresponds to uncompressed.
std::string CreateKVv2ResponseBody(
    std::string_view cbor_response_body,
    std::optional<size_t> advertised_cbor_length = std::nullopt,
    size_t padding_length = 0,
    uint8_t compression_scheme = 0);

}  // namespace auction_worklet::test

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_CBOR_TEST_UTIL_H_
