// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_STRING_UTIL_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_STRING_UTIL_H_

// clang-format off: Must precede C++ standard library headers.
#include "sandbox/win/tests/basic_process/shim/libcxx_support.h"
// clang-format on

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

namespace basic_process {

template <typename Char>
constexpr Char ToLowerAscii(Char c) {
  return c >= static_cast<Char>('A') && c <= static_cast<Char>('Z')
             ? static_cast<Char>(c + ('a' - 'A'))
             : c;
}

// Folds only ASCII letters and compares unsigned code units. This does not
// perform character encoding conversion.
template <typename LhsChar, typename RhsChar>
constexpr bool EqualsAsciiIgnoringCase(std::basic_string_view<LhsChar> lhs,
                                       std::basic_string_view<RhsChar> rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  using ComparisonChar = std::common_type_t<std::make_unsigned_t<LhsChar>,
                                            std::make_unsigned_t<RhsChar>>;
  for (size_t i = 0; i < lhs.size(); ++i) {
    const ComparisonChar left =
        static_cast<std::make_unsigned_t<LhsChar>>(ToLowerAscii(lhs[i]));
    const ComparisonChar right =
        static_cast<std::make_unsigned_t<RhsChar>>(ToLowerAscii(rhs[i]));
    if (left != right) {
      return false;
    }
  }
  return true;
}

// Both slash styles are separators. The returned view borrows path.
template <typename Char>
constexpr std::basic_string_view<Char> BaseName(
    std::basic_string_view<Char> path) {
  if (path.empty()) {
    return path;
  }
  size_t begin = path.size();
  while (begin > 0) {
    const Char c = path[begin - 1];
    if (c == static_cast<Char>('/') || c == static_cast<Char>('\\')) {
      break;
    }
    --begin;
  }
  return path.substr(begin);
}

inline constexpr size_t kMaxUint64DecimalDigits =
    std::numeric_limits<uint64_t>::digits10 + 1;
inline constexpr size_t kMaxUint64HexChars =
    2 + std::numeric_limits<uint64_t>::digits / 4;

// The returned view refers to output and does not include a NUL terminator.
std::wstring_view FormatDecimal(
    uint64_t value,
    std::array<wchar_t, kMaxUint64DecimalDigits>& output);

// Uses a lowercase "0x" prefix and minimal-width digits, without a terminator.
std::wstring_view FormatHex(uint64_t value,
                            std::array<wchar_t, kMaxUint64HexChars>& output);

// Skips an opening quote and stops at a closing quote, or unquoted space/tab.
// quoted_argument handles values whose enclosing argument was already quoted.
// This only slices text; it does not match switches or unescape arguments.
std::wstring_view ExtractQuotedValue(std::wstring_view value,
                                     bool quoted_argument);

// 64-bit FNV-1a, a fast non-cryptographic hash that needs no tables or
// allocation. For each input byte it XORs the byte into the hash and then
// multiplies by the FNV prime. The offset basis and prime are the published
// 64-bit FNV parameters:
// https://datatracker.ietf.org/doc/html/draft-eastlake-fnv#name-fnv-constants
// Use it only where collisions are tolerable, such as deduplicating log
// records.
class Fnv1aHasher {
 public:
  static constexpr uint64_t kOffsetBasis = 0xcbf29ce484222325;
  // 2^40 + 2^8 + 0xb3.
  static constexpr uint64_t kPrime = 0x100000001b3;

  constexpr void AddByte(uint8_t byte) { hash_ = (hash_ ^ byte) * kPrime; }

  // Adds an integer's bytes, least significant first.
  template <typename Int>
  constexpr void AddLittleEndian(Int value) {
    static_assert(std::is_integral_v<Int>);
    const auto bits = static_cast<std::make_unsigned_t<Int>>(value);
    for (size_t i = 0; i < sizeof(Int); ++i) {
      AddByte(static_cast<uint8_t>(bits >> (8 * i)));
    }
  }

  // Adds each code unit with AddLittleEndian(), so wide strings contribute
  // two bytes per UTF-16 code unit.
  constexpr void AddString(std::string_view value) { AddCodeUnits(value); }
  constexpr void AddString(std::wstring_view value) { AddCodeUnits(value); }

  constexpr uint64_t value() const { return hash_; }

 private:
  template <typename Char>
  constexpr void AddCodeUnits(std::basic_string_view<Char> value) {
    for (Char c : value) {
      AddLittleEndian(c);
    }
  }

  uint64_t hash_ = kOffsetBasis;
};

// Fixed storage for no-CRT code. Storage includes the NUL terminator, which is
// maintained after every mutation. TryAppend operations never partially write;
// diagnostic callers can explicitly choose AppendPrefix instead.
template <size_t kStorageSize>
class WideStringBuffer {
  static_assert(kStorageSize > 0);

 public:
  WideStringBuffer() { buffer_[0] = L'\0'; }
  WideStringBuffer(const WideStringBuffer&) = delete;
  WideStringBuffer& operator=(const WideStringBuffer&) = delete;

  [[nodiscard]] bool TryAppend(std::wstring_view value) {
    if (value.size() > remaining()) {
      return false;
    }
    AppendPrefix(value);
    return true;
  }

  [[nodiscard]] bool TryAppend(wchar_t value) {
    if (!remaining()) {
      return false;
    }
    buffer_[size_++] = value;
    buffer_[size_] = L'\0';
    return true;
  }

  [[nodiscard]] bool TryAppendDecimal(uint64_t value) {
    std::array<wchar_t, kMaxUint64DecimalDigits> digits;
    return TryAppend(FormatDecimal(value, digits));
  }

  // Appends as much as fits and returns the number of characters appended.
  size_t AppendPrefix(std::wstring_view value) {
    const size_t count =
        value.size() < remaining() ? value.size() : remaining();
    for (size_t i = 0; i < count; ++i) {
      buffer_[size_ + i] = value[i];
    }
    size_ += count;
    buffer_[size_] = L'\0';
    return count;
  }

  // Shortens the string to at most max_length; never grows it.
  void Truncate(size_t max_length) {
    if (max_length < size_) {
      size_ = max_length;
      buffer_[size_] = L'\0';
    }
  }

  void Clear() { Truncate(0); }

  std::wstring_view view() const {
    return std::wstring_view(buffer_.begin(), buffer_.end()).substr(0, size_);
  }

  const wchar_t* c_str() const { return buffer_.data(); }
  size_t size() const { return size_; }
  static constexpr size_t capacity() { return kStorageSize - 1; }
  size_t remaining() const { return capacity() - size_; }

 private:
  std::array<wchar_t, kStorageSize> buffer_;
  size_t size_ = 0;
};

}  // namespace basic_process

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_STRING_UTIL_H_
