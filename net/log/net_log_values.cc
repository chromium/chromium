// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log_values.h"

#include "base/base64.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace net {

namespace {

// IEEE 64-bit doubles have a 52-bit mantissa, and can therefore represent
// 53-bits worth of precision (see also documentation for JavaScript's
// Number.MAX_SAFE_INTEGER for more discussion on this).
//
// If the number can be represented with an int or double use that. Otherwise
// fallback to encoding it as a string.
template <typename T>
base::Value NetLogNumberValueHelper(T num) {
  // Fits in a (32-bit) int: [-2^31, 2^31 - 1]
  if ((!std::is_signed<T>::value || (num >= static_cast<T>(-2147483648))) &&
      (num <= static_cast<T>(2147483647))) {
    return base::Value(static_cast<int>(num));
  }

  // Fits in a double: (-2^53, 2^53)
  if ((!std::is_signed<T>::value ||
       (num >= static_cast<T>(-9007199254740991))) &&
      (num <= static_cast<T>(9007199254740991))) {
    return base::Value(static_cast<double>(num));
  }

  // Otherwise format as a string.
  return base::Value(base::NumberToString(num));
}

}  // namespace

base::Value NetLogStringValue(std::string_view raw) {
  // The common case is that |raw| is ASCII. Represent this directly.
  if (base::IsStringASCII(raw))
    return base::Value(raw);

  // For everything else (including valid UTF-8) percent-escape |raw|, and add a
  // prefix that "tags" the value as being a percent-escaped representation.
  //
  // Note that the sequence E2 80 8B is U+200B (zero-width space) in UTF-8. It
  // is added so the escaped string is not itself also ASCII (otherwise there
  // would be ambiguity for consumers as to when the value needs to be
  // unescaped).
  return base::Value("%ESCAPED:\xE2\x80\x8B " +
                     base::EscapeNonASCIIAndPercent(raw));
}

base::Value NetLogBinaryValue(base::span<const uint8_t> bytes) {
  return NetLogBinaryValue(bytes.data(), bytes.size());
}

base::Value NetLogBinaryValue(const void* bytes, size_t length) {
  std::string b64 = base::Base64Encode(
      std::string_view(reinterpret_cast<const char*>(bytes), length));
  return base::Value(std::move(b64));
}

base::Value NetLogNumberValue(int64_t num) {
  return NetLogNumberValueHelper(num);
}

base::Value NetLogNumberValue(uint64_t num) {
  return NetLogNumberValueHelper(num);
}

base::Value NetLogNumberValue(uint32_t num) {
  return NetLogNumberValueHelper(num);
}

base::Value::Dict NetLogParamsWithInt(std::string_view name, int value) {
  base::Value::Dict params;
  params.Set(name, value);
  return params;
}

base::Value::Dict NetLogParamsWithInt64(std::string_view name, int64_t value) {
  base::Value::Dict params;
  params.Set(name, NetLogNumberValue(value));
  return params;
}

base::Value::Dict NetLogParamsWithBool(std::string_view name, bool value) {
  base::Value::Dict params;
  params.Set(name, value);
  return params;
}

base::Value::Dict NetLogParamsWithString(std::string_view name,
                                         std::string_view value) {
  base::Value::Dict params;
  params.Set(name, value);
  return params;
}

}  // namespace net
