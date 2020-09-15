// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEXT_UTILS_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEXT_UTILS_IMPL_H_

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "base/strings/abseil_string_conversions.h"
#include "net/base/hex_utils.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_optional.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/abseil-cpp/absl/strings/escaping.h"
#include "third_party/abseil-cpp/absl/strings/match.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"
#include "third_party/abseil-cpp/absl/strings/str_split.h"

namespace quiche {

// Chromium implementation of quiche::QuicheTextUtils.
class QuicheTextUtilsImpl {
 public:
  // Returns true of |data| starts with |prefix|, case sensitively.
  static bool StartsWith(QuicheStringPiece data, QuicheStringPiece prefix) {
    return absl::StartsWith(data, prefix);
  }

  // Returns true if |data| end with |suffix|, case sensitively.
  static bool EndsWith(QuicheStringPiece data, QuicheStringPiece suffix) {
    return absl::EndsWith(data, suffix);
  }

  // Returns true of |data| ends with |suffix|, case insensitively.
  static bool EndsWithIgnoreCase(QuicheStringPiece data,
                                 QuicheStringPiece suffix) {
    return absl::EndsWithIgnoreCase(data, suffix);
  }

  // Returns a new std::string in which |data| has been converted to lower case.
  static std::string ToLower(QuicheStringPiece data) {
    return absl::AsciiStrToLower(data);
  }

  // Remove leading and trailing whitespace from |data|.
  static void RemoveLeadingAndTrailingWhitespace(QuicheStringPiece* data) {
    *data = absl::StripAsciiWhitespace(*data);
  }

  // Returns true if |in| represents a valid uint64, and stores that value in
  // |out|.
  static bool StringToUint64(QuicheStringPiece in, uint64_t* out) {
    return absl::SimpleAtoi(in, out);
  }

  // Returns true if |in| represents a valid int, and stores that value in
  // |out|.
  static bool StringToInt(QuicheStringPiece in, int* out) {
    return absl::SimpleAtoi(in, out);
  }

  // Returns true if |in| represents a valid uint32, and stores that value in
  // |out|.
  static bool StringToUint32(QuicheStringPiece in, uint32_t* out) {
    return absl::SimpleAtoi(in, out);
  }

  // Returns true if |in| represents a valid size_t, and stores that value in
  // |out|.
  static bool StringToSizeT(QuicheStringPiece in, size_t* out) {
    return absl::SimpleAtoi(in, out);
  }

  // Returns a new std::string representing |in|.
  static std::string Uint64ToString(uint64_t in) { return absl::StrCat(in); }

  // This converts |length| bytes of binary to a 2*|length|-character
  // hexadecimal representation.
  // Return value: 2*|length| characters of ASCII std::string.
  static std::string HexEncode(QuicheStringPiece data) {
    return absl::BytesToHexString(data);
  }

  static std::string Hex(uint32_t v) { return absl::StrCat(absl::Hex(v)); }

  // Converts |data| from a hexadecimal ASCII string to a binary string
  // that is |data.length()/2| bytes long. On failure returns empty string.
  static std::string HexDecode(QuicheStringPiece data) {
    return absl::HexStringToBytes(data);
  }

  // Base64 encodes with no padding |data_len| bytes of |data| into |output|.
  static void Base64Encode(const uint8_t* data,
                           size_t data_len,
                           std::string* output) {
    absl::Base64Escape(
        std::string(reinterpret_cast<const char*>(data), data_len), output);
    // Remove padding.
    size_t len = output->size();
    if (len >= 2) {
      if ((*output)[len - 1] == '=') {
        len--;
        if ((*output)[len - 1] == '=') {
          len--;
        }
        output->resize(len);
      }
    }
  }

  // Decodes a base64-encoded |input|.  Returns nullopt when the input is
  // invalid.
  static QuicheOptional<std::string> Base64Decode(QuicheStringPiece input) {
    std::string output;
    if (!absl::Base64Unescape(input, &output)) {
      return QuicheOptional<std::string>();
    }
    return output;
  }

  // Returns a std::string containing hex and ASCII representations of |binary|,
  // side-by-side in the style of hexdump. Non-printable characters will be
  // printed as '.' in the ASCII output.
  // For example, given the input "Hello, QUIC!\01\02\03\04", returns:
  // "0x0000:  4865 6c6c 6f2c 2051 5549 4321 0102 0304  Hello,.QUIC!...."
  static std::string HexDump(QuicheStringPiece binary_input) {
    return net::HexDump(base::StringViewToStringPiece(binary_input));
  }

  // Returns true if |data| contains any uppercase characters.
  static bool ContainsUpperCase(QuicheStringPiece data) {
    return std::any_of(data.begin(), data.end(), absl::ascii_isupper);
  }

  // Returns true if |data| contains only decimal digits.
  static bool IsAllDigits(QuicheStringPiece data) {
    return std::all_of(data.begin(), data.end(), absl::ascii_isdigit);
  }

  // Splits |data| into a vector of pieces delimited by |delim|.
  static std::vector<QuicheStringPiece> Split(QuicheStringPiece data,
                                              char delim) {
    return absl::StrSplit(data, delim);
  }
};

}  // namespace quiche

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEXT_UTILS_IMPL_H_
