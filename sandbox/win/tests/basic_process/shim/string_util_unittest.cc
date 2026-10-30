// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/shim/string_util.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace basic_process {
namespace {

using DecimalStorage = std::array<wchar_t, kMaxUint64DecimalDigits>;
using HexStorage = std::array<wchar_t, kMaxUint64HexChars>;

// Formatter results borrow correctly sized, caller-owned storage.
static_assert(
    std::is_invocable_v<decltype(FormatDecimal), uint64_t, DecimalStorage&>);
static_assert(
    !std::is_invocable_v<decltype(FormatDecimal),
                         uint64_t,
                         std::array<wchar_t, kMaxUint64DecimalDigits - 1>&>);
static_assert(
    !std::is_invocable_v<decltype(FormatDecimal),
                         uint64_t,
                         std::array<wchar_t, kMaxUint64DecimalDigits + 1>&>);
static_assert(
    !std::is_invocable_v<decltype(FormatDecimal), uint64_t, DecimalStorage>);
static_assert(std::is_invocable_v<decltype(FormatHex), uint64_t, HexStorage&>);
static_assert(
    !std::is_invocable_v<decltype(FormatHex),
                         uint64_t,
                         std::array<wchar_t, kMaxUint64HexChars - 1>&>);
static_assert(
    !std::is_invocable_v<decltype(FormatHex),
                         uint64_t,
                         std::array<wchar_t, kMaxUint64HexChars + 1>&>);
static_assert(!std::is_invocable_v<decltype(FormatHex), uint64_t, HexStorage>);

TEST(WideStringBufferTest, EmptyAndTerminatorOnlyStorage) {
  WideStringBuffer<1> buffer;
  EXPECT_TRUE(buffer.view().empty());
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.capacity(), 0u);
  EXPECT_EQ(buffer.remaining(), 0u);
  EXPECT_STREQ(buffer.c_str(), L"");
  EXPECT_TRUE(buffer.TryAppend(L""));
  EXPECT_FALSE(buffer.TryAppend(L'a'));
  EXPECT_FALSE(buffer.TryAppendDecimal(0));
  EXPECT_EQ(buffer.AppendPrefix(L"abc"), 0u);
  EXPECT_TRUE(buffer.view().empty());
}

TEST(WideStringBufferTest, ExactFitRemainsTerminated) {
  WideStringBuffer<4> buffer;
  EXPECT_TRUE(buffer.TryAppend(L"abc"));
  EXPECT_EQ(buffer.view(), L"abc");
  EXPECT_EQ(buffer.size(), 3u);
  EXPECT_EQ(buffer.remaining(), 0u);
  EXPECT_STREQ(buffer.c_str(), L"abc");
  EXPECT_TRUE(buffer.TryAppend(L""));
  EXPECT_FALSE(buffer.TryAppend(L'd'));
  EXPECT_FALSE(buffer.TryAppend(L"de"));
  EXPECT_EQ(buffer.view(), L"abc");
  EXPECT_STREQ(buffer.c_str(), L"abc");
}

TEST(WideStringBufferTest, FailedAppendDoesNotWriteAPrefix) {
  WideStringBuffer<5> buffer;
  ASSERT_TRUE(buffer.TryAppend(L"ab"));
  EXPECT_FALSE(buffer.TryAppend(L"cde"));
  EXPECT_EQ(buffer.view(), L"ab");
  EXPECT_EQ(buffer.remaining(), 2u);
  EXPECT_TRUE(buffer.TryAppend(L"cd"));
  EXPECT_EQ(buffer.view(), L"abcd");
}

TEST(WideStringBufferTest, ExplicitlyTruncatedAppend) {
  WideStringBuffer<5> buffer;
  ASSERT_TRUE(buffer.TryAppend(L"a"));
  EXPECT_EQ(buffer.AppendPrefix(L"bcdef"), 3u);
  EXPECT_EQ(buffer.view(), L"abcd");
  EXPECT_EQ(buffer.AppendPrefix(L"e"), 0u);
  EXPECT_STREQ(buffer.c_str(), L"abcd");
}

TEST(WideStringBufferTest, TruncateAndReuse) {
  WideStringBuffer<8> buffer;
  ASSERT_TRUE(buffer.TryAppend(L"abcdef"));
  buffer.Truncate(3);
  EXPECT_EQ(buffer.view(), L"abc");
  EXPECT_STREQ(buffer.c_str(), L"abc");
  buffer.Truncate(100);
  EXPECT_EQ(buffer.view(), L"abc");
  ASSERT_TRUE(buffer.TryAppend(L"defg"));
  EXPECT_EQ(buffer.view(), L"abcdefg");
  buffer.Clear();
  EXPECT_TRUE(buffer.view().empty());
  EXPECT_STREQ(buffer.c_str(), L"");
  EXPECT_EQ(buffer.remaining(), buffer.capacity());
  ASSERT_TRUE(buffer.TryAppend(L'z'));
  EXPECT_EQ(buffer.view(), L"z");
}

TEST(WideStringBufferTest, DecimalAppendIsAtomic) {
  WideStringBuffer<4> buffer;
  ASSERT_TRUE(buffer.TryAppend(L"ab"));
  EXPECT_FALSE(buffer.TryAppendDecimal(99));
  EXPECT_EQ(buffer.view(), L"ab");
  EXPECT_TRUE(buffer.TryAppendDecimal(7));
  EXPECT_EQ(buffer.view(), L"ab7");
  EXPECT_STREQ(buffer.c_str(), L"ab7");
}

TEST(WideStringBufferTest, IntegerLimitsFitExactly) {
  WideStringBuffer<11> dword;
  EXPECT_TRUE(dword.TryAppendDecimal(4294967295ULL));
  EXPECT_EQ(dword.view(), L"4294967295");
  EXPECT_EQ(dword.remaining(), 0u);
  WideStringBuffer<kMaxUint64DecimalDigits + 1> uint64;
  EXPECT_TRUE(uint64.TryAppendDecimal(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(uint64.view(), L"18446744073709551615");
  EXPECT_EQ(uint64.remaining(), 0u);
}

TEST(WideStringBufferTest, AppendViewUsesItsLength) {
  WideStringBuffer<5> buffer;
  const std::wstring_view input = L"abcde";
  ASSERT_TRUE(buffer.TryAppend(input.substr(1, 3)));
  EXPECT_EQ(buffer.view(), L"bcd");
  EXPECT_STREQ(buffer.c_str(), L"bcd");
}

TEST(FormatDecimalTest, DecimalBoundaries) {
  const struct {
    uint64_t value;
    std::wstring_view expected;
  } cases[] = {
      {0, L"0"},
      {1, L"1"},
      {9, L"9"},
      {10, L"10"},
      {99, L"99"},
      {100, L"100"},
      {999, L"999"},
      {1000, L"1000"},
      {999999999, L"999999999"},
      {1000000000, L"1000000000"},
      {4294967295ULL, L"4294967295"},
      {4294967296ULL, L"4294967296"},
      {999999999999999999ULL, L"999999999999999999"},
      {1000000000000000000ULL, L"1000000000000000000"},
      {9999999999999999999ULL, L"9999999999999999999"},
      {10000000000000000000ULL, L"10000000000000000000"},
      {18446744073709551615ULL, L"18446744073709551615"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    std::array<wchar_t, kMaxUint64DecimalDigits> output;
    EXPECT_EQ(FormatDecimal(test.value, output), test.expected);
  }
}

TEST(AsciiCaseTest, FoldsOnlyAsciiLetters) {
  EXPECT_EQ(ToLowerAscii('A'), 'a');
  EXPECT_EQ(ToLowerAscii('Z'), 'z');
  EXPECT_EQ(ToLowerAscii('a'), 'a');
  EXPECT_EQ(ToLowerAscii('['), '[');
  EXPECT_EQ(ToLowerAscii('\0'), '\0');
  EXPECT_EQ(ToLowerAscii(static_cast<char>(0xff)), static_cast<char>(0xff));
  EXPECT_EQ(ToLowerAscii(L'B'), L'b');
  EXPECT_EQ(ToLowerAscii(L'\u00c9'), L'\u00c9');
}

TEST(AsciiCaseTest, ComparesNarrowAndWideViews) {
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::string_view("Kernel32.DLL"),
                                      std::string_view("kernel32.dll")));
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::wstring_view(L"Kernel32"),
                                      std::string_view("kernel32")));
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::string_view("kernel32"),
                                      std::wstring_view(L"KERNEL32")));
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::string_view(), std::wstring_view()));
  EXPECT_FALSE(EqualsAsciiIgnoringCase(std::wstring_view(L"kernel32"),
                                       std::string_view("kernel32.dll")));
  EXPECT_FALSE(EqualsAsciiIgnoringCase(std::string_view("kernelbase"),
                                       std::string_view("kernel32")));
  const std::wstring_view bounded = L"KERNEL32 extra";
  EXPECT_TRUE(EqualsAsciiIgnoringCase(bounded.substr(0, 8),
                                      std::string_view("kernel32")));
}

TEST(AsciiCaseTest, NonAsciiCodeUnitsAreNotCaseFoldedOrDecoded) {
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::string_view("\xff"),
                                      std::string_view("\xff")));
  EXPECT_TRUE(EqualsAsciiIgnoringCase(std::string_view("\xff"),
                                      std::wstring_view(L"\u00ff")));
  EXPECT_FALSE(EqualsAsciiIgnoringCase(std::wstring_view(L"\u00c9"),
                                       std::wstring_view(L"\u00e9")));
  EXPECT_FALSE(EqualsAsciiIgnoringCase(std::string_view("\xc3\xa9"),
                                       std::wstring_view(L"\u00e9")));
}

TEST(BaseNameTest, NarrowPaths) {
  const struct {
    std::string_view path;
    std::string_view expected;
  } cases[] = {
      {"", ""},
      {"kernel32.dll", "kernel32.dll"},
      {"C:\\Windows\\System32\\kernel32.dll", "kernel32.dll"},
      {"C:/Windows/System32/kernel32.dll", "kernel32.dll"},
      {"mixed\\directory/file.dll", "file.dll"},
      {"\\\\server\\share\\file.dll", "file.dll"},
      {"directory\\", ""},
      {"/", ""},
      {"C:", "C:"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.path);
    EXPECT_EQ(BaseName(test.path), test.expected);
  }
  EXPECT_TRUE(BaseName(std::string_view()).empty());
}

TEST(BaseNameTest, WidePaths) {
  const struct {
    std::wstring_view path;
    std::wstring_view expected;
  } cases[] = {
      {L"", L""},
      {L"kernel32.dll", L"kernel32.dll"},
      {L"C:\\Windows\\System32\\kernel32.dll", L"kernel32.dll"},
      {L"C:/Windows/System32/kernel32.dll", L"kernel32.dll"},
      {L"mixed\\directory/file.dll", L"file.dll"},
      {L"\\\\server\\share\\file.dll", L"file.dll"},
      {L"directory\\", L""},
      {L"/", L""},
      {L"C:", L"C:"},
  };
  for (const auto& test : cases) {
    EXPECT_EQ(BaseName(test.path), test.expected);
  }
  EXPECT_TRUE(BaseName(std::wstring_view()).empty());
}

TEST(BaseNameTest, SubviewsDoNotRequireTerminators) {
  const std::string_view narrow = "dir/file.dll?ignored";
  EXPECT_EQ(BaseName(narrow.substr(0, narrow.find('?'))), "file.dll");
  const std::wstring_view wide = L"dir\\file.dll?ignored";
  EXPECT_EQ(BaseName(wide.substr(0, wide.find(L'?'))), L"file.dll");
}

TEST(FormatHexTest, IntegerBoundariesAndStorageBounds) {
  const struct {
    uint64_t value;
    std::wstring_view expected;
  } cases[] = {
      {0, L"0x0"},
      {1, L"0x1"},
      {15, L"0xf"},
      {16, L"0x10"},
      {255, L"0xff"},
      {256, L"0x100"},
      {4294967295ULL, L"0xffffffff"},
      {4294967296ULL, L"0x100000000"},
      {18446744073709551615ULL, L"0xffffffffffffffff"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.value);
    struct {
      wchar_t before = L'#';
      std::array<wchar_t, kMaxUint64HexChars> output;
      wchar_t after = L'#';
    } storage;
    EXPECT_EQ(FormatHex(test.value, storage.output), test.expected);
    EXPECT_EQ(storage.before, L'#');
    EXPECT_EQ(storage.after, L'#');
  }
}

TEST(ExtractQuotedValueTest, QuotingAndDelimiters) {
  const struct {
    std::wstring_view input;
    bool quoted_argument;
    std::wstring_view expected;
  } cases[] = {
      {L"", false, L""},
      {L"plain", false, L"plain"},
      {L"plain next", false, L"plain"},
      {L"plain\tnext", false, L"plain"},
      {L" next", false, L""},
      {L"\"path with spaces\" next", false, L"path with spaces"},
      {L"path with spaces\" next", true, L"path with spaces"},
      {L"\"\"", false, L""},
      {L"\"unterminated path", false, L"unterminated path"},
      {L"unterminated path", true, L"unterminated path"},
  };
  for (const auto& test : cases) {
    EXPECT_EQ(ExtractQuotedValue(test.input, test.quoted_argument),
              test.expected);
  }
}

TEST(ExtractQuotedValueTest, DoesNotUnescapeOrInterpretEmbeddedQuotes) {
  EXPECT_EQ(ExtractQuotedValue(L"\"a\\\"b\"", false), L"a\\");
  EXPECT_EQ(ExtractQuotedValue(L"plain\"quote\"", false), L"plain\"quote\"");
}

uint64_t HashBytes(std::initializer_list<uint8_t> bytes) {
  Fnv1aHasher hasher;
  for (uint8_t byte : bytes) {
    hasher.AddByte(byte);
  }
  return hasher.value();
}

TEST(Fnv1aHasherTest, MatchesPublishedValues) {
  // 64-bit FNV-1a values for "", "a" and "foobar" from the FNV reference.
  EXPECT_EQ(Fnv1aHasher().value(), 0xcbf29ce484222325u);
  Fnv1aHasher a;
  a.AddString(std::string_view("a"));
  EXPECT_EQ(a.value(), 0xaf63dc4c8601ec8cu);
  Fnv1aHasher foobar;
  foobar.AddString(std::string_view("foobar"));
  EXPECT_EQ(foobar.value(), 0x85944171f73967e8u);
}

TEST(Fnv1aHasherTest, AddsIntegerBytesLeastSignificantFirst) {
  Fnv1aHasher value;
  value.AddLittleEndian(uint64_t{0x0807060504030201});
  EXPECT_EQ(value.value(), HashBytes({1, 2, 3, 4, 5, 6, 7, 8}));

  Fnv1aHasher wide;
  wide.AddString(std::wstring_view(L"a\x0102"));
  EXPECT_EQ(wide.value(), HashBytes({0x61, 0x00, 0x02, 0x01}));
}

}  // namespace
}  // namespace basic_process
