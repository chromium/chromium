// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/shim/string_util.h"

namespace basic_process {

std::wstring_view FormatDecimal(
    uint64_t value,
    std::array<wchar_t, kMaxUint64DecimalDigits>& output) {
  size_t begin = output.size();
  do {
    output[--begin] = static_cast<wchar_t>(L'0' + value % 10);
    value /= 10;
  } while (value);
  return std::wstring_view(output.begin(), output.end()).substr(begin);
}

std::wstring_view FormatHex(uint64_t value,
                            std::array<wchar_t, kMaxUint64HexChars>& output) {
  constexpr std::wstring_view kDigits = L"0123456789abcdef";
  size_t begin = output.size();
  do {
    output[--begin] = kDigits[value & 0xf];
    value >>= 4;
  } while (value);
  output[--begin] = L'x';
  output[--begin] = L'0';
  return std::wstring_view(output.begin(), output.end()).substr(begin);
}

std::wstring_view ExtractQuotedValue(std::wstring_view value,
                                     bool quoted_argument) {
  if (value.starts_with(L'"')) {
    quoted_argument = true;
    value.remove_prefix(1);
  }
  const size_t end = value.find_first_of(quoted_argument ? L"\"" : L" \t");
  return end == std::wstring_view::npos ? value : value.substr(0, end);
}

}  // namespace basic_process
