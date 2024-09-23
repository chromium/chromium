/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace WTF {

TEST(StringTest, CreationFromLiteral) {
  String string_from_literal("Explicit construction syntax");
  EXPECT_EQ(strlen("Explicit construction syntax"),
            string_from_literal.length());
  EXPECT_TRUE(string_from_literal == "Explicit construction syntax");
  EXPECT_TRUE(string_from_literal.Is8Bit());
  EXPECT_TRUE(String("Explicit construction syntax") == string_from_literal);
}

TEST(StringTest, CreationFromHashTraits) {
  String zero;
  EXPECT_TRUE(zero.IsNull());
  EXPECT_TRUE(zero.empty());
  EXPECT_TRUE(HashTraits<String>::IsEmptyValue(zero));
  EXPECT_EQ(zero, HashTraits<String>::EmptyValue());

  String empty = "";
  EXPECT_FALSE(empty.IsNull());
  EXPECT_TRUE(empty.empty());
  EXPECT_FALSE(HashTraits<String>::IsEmptyValue(empty));
  EXPECT_NE(empty, HashTraits<String>::EmptyValue());

  uint32_t hash = String("abc").Impl()->GetHash();
  EXPECT_EQ(hash, HashTraits<String>::GetHash(String("abc")));
  EXPECT_EQ(hash, HashTraits<String>::GetHash("abc"));
  EXPECT_EQ(hash,
            HashTraits<String>::GetHash(reinterpret_cast<const LChar*>("abc")));
  const UChar abc_wide[] = {'a', 'b', 'c', 0};
  EXPECT_EQ(hash, HashTraits<String>::GetHash(abc_wide));
}

TEST(StringTest, EqualHashTraits) {
  String abc = "abc";
  String def = "def";

  EXPECT_TRUE(HashTraits<String>::Equal(abc, abc));
  EXPECT_FALSE(HashTraits<String>::Equal(abc, def));

  EXPECT_TRUE(HashTraits<String>::Equal(abc, "abc"));
  EXPECT_FALSE(HashTraits<String>::Equal(abc, "def"));
  EXPECT_TRUE(HashTraits<String>::Equal("abc", abc));
  EXPECT_FALSE(HashTraits<String>::Equal("def", abc));

  EXPECT_TRUE(
      HashTraits<String>::Equal(abc, reinterpret_cast<const LChar*>("abc")));
  EXPECT_FALSE(
      HashTraits<String>::Equal(abc, reinterpret_cast<const LChar*>("def")));
  EXPECT_TRUE(
      HashTraits<String>::Equal(reinterpret_cast<const LChar*>("abc"), abc));
  EXPECT_FALSE(
      HashTraits<String>::Equal(reinterpret_cast<const LChar*>("def"), abc));

  const UChar abc_wide[] = {'a', 'b', 'c', 0};
  const UChar def_wide[] = {'d', 'e', 'f', 0};
  EXPECT_TRUE(HashTraits<String>::Equal(abc, abc_wide));
  EXPECT_FALSE(HashTraits<String>::Equal(abc, def_wide));
  EXPECT_TRUE(HashTraits<String>::Equal(abc_wide, abc));
  EXPECT_FALSE(HashTraits<String>::Equal(def_wide, abc));
}

TEST(StringTest, ASCII) {
  // Null String.
  EXPECT_EQ("", String().Ascii());

  // Empty String.
  EXPECT_EQ("", g_empty_string.Ascii());

  // Regular String.
  EXPECT_EQ("foobar", String("foobar").Ascii());
}

namespace {

void TestNumberToStringECMAScript(double number, const char* reference) {
  EXPECT_EQ(reference, String::NumberToStringECMAScript(number));
}

}  // anonymous namespace

TEST(StringTest, NumberToStringECMAScriptBoundaries) {
  typedef std::numeric_limits<double> Limits;

  // Infinity.
  TestNumberToStringECMAScript(Limits::infinity(), "Infinity");
  TestNumberToStringECMAScript(-Limits::infinity(), "-Infinity");

  // NaN.
  TestNumberToStringECMAScript(-Limits::quiet_NaN(), "NaN");

  // Zeros.
  TestNumberToStringECMAScript(0, "0");
  TestNumberToStringECMAScript(-0, "0");

  // Min-Max.
  TestNumberToStringECMAScript(Limits::min(), "2.2250738585072014e-308");
  TestNumberToStringECMAScript(Limits::max(), "1.7976931348623157e+308");
}

TEST(StringTest, NumberToStringECMAScriptRegularNumbers) {
  // Pi.
  TestNumberToStringECMAScript(kPiDouble, "3.141592653589793");
  TestNumberToStringECMAScript(kPiFloat, "3.1415927410125732");
  TestNumberToStringECMAScript(kPiOverTwoDouble, "1.5707963267948966");
  TestNumberToStringECMAScript(kPiOverTwoFloat, "1.5707963705062866");
  TestNumberToStringECMAScript(kPiOverFourDouble, "0.7853981633974483");
  TestNumberToStringECMAScript(kPiOverFourFloat, "0.7853981852531433");

  // e.
  const double kE = 2.71828182845904523536028747135266249775724709369995;
  TestNumberToStringECMAScript(kE, "2.718281828459045");

  // c, speed of light in m/s.
  const double kC = 299792458;
  TestNumberToStringECMAScript(kC, "299792458");

  // Golen ratio.
  const double kPhi = 1.6180339887498948482;
  TestNumberToStringECMAScript(kPhi, "1.618033988749895");
}

TEST(StringTest, ReplaceWithLiteral) {
  // Cases for 8Bit source.
  String test_string = "1224";
  EXPECT_TRUE(test_string.Is8Bit());
  test_string.Replace('2', "");
  EXPECT_EQ("14", test_string);

  test_string = "1224";
  EXPECT_TRUE(test_string.Is8Bit());
  test_string.Replace('2', "3");
  EXPECT_EQ("1334", test_string);

  test_string = "1224";
  EXPECT_TRUE(test_string.Is8Bit());
  test_string.Replace('2', "555");
  EXPECT_EQ("15555554", test_string);

  test_string = "1224";
  EXPECT_TRUE(test_string.Is8Bit());
  test_string.Replace('3', "NotFound");
  EXPECT_EQ("1224", test_string);

  // Cases for 16Bit source.
  // U+00E9 (=0xC3 0xA9 in UTF-8) is e with accent.
  test_string = String::FromUTF8("r\xC3\xA9sum\xC3\xA9");
  EXPECT_FALSE(test_string.Is8Bit());
  test_string.Replace(UChar(0x00E9), "e");
  EXPECT_EQ("resume", test_string);

  test_string = String::FromUTF8("r\xC3\xA9sum\xC3\xA9");
  EXPECT_FALSE(test_string.Is8Bit());
  test_string.Replace(UChar(0x00E9), "");
  EXPECT_EQ("rsum", test_string);

  test_string = String::FromUTF8("r\xC3\xA9sum\xC3\xA9");
  EXPECT_FALSE(test_string.Is8Bit());
  test_string.Replace('3', "NotFound");
  EXPECT_EQ("r\xC3\xA9sum\xC3\xA9", test_string.Utf8());
}

TEST(StringTest, ComparisonOfSameStringVectors) {
  Vector<String> string_vector;
  string_vector.push_back("one");
  string_vector.push_back("two");

  Vector<String> same_string_vector;
  same_string_vector.push_back("one");
  same_string_vector.push_back("two");

  EXPECT_EQ(string_vector, same_string_vector);
}

TEST(WTF, LengthWithStrippedWhiteSpace) {
  String stripped("Hello  world");
  EXPECT_EQ(stripped.LengthWithStrippedWhiteSpace(), stripped.length());
  EXPECT_EQ(String("  Hello  world  ").LengthWithStrippedWhiteSpace(),
            stripped.length());
  EXPECT_EQ(String("Hello  world  ").LengthWithStrippedWhiteSpace(),
            stripped.length());
  EXPECT_EQ(String("  Hello  world").LengthWithStrippedWhiteSpace(),
            stripped.length());
  EXPECT_EQ(String("\nHello\n world  ").LengthWithStrippedWhiteSpace(),
            stripped.length());
  EXPECT_EQ(String().LengthWithStrippedWhiteSpace(), 0u);
  EXPECT_EQ(String("").LengthWithStrippedWhiteSpace(), 0u);
  EXPECT_EQ(String("\n").LengthWithStrippedWhiteSpace(), 0u);
  EXPECT_EQ(String("\n\n").LengthWithStrippedWhiteSpace(), 0u);
  String only_spaces("   ");
  EXPECT_EQ(only_spaces.LengthWithStrippedWhiteSpace(), 0u);
}

TEST(WTF, SimplifyWhiteSpace) {
  String extra_spaces("  Hello  world  ");
  EXPECT_EQ(String("Hello world"), extra_spaces.SimplifyWhiteSpace());
  EXPECT_EQ(String("  Hello  world  "),
            extra_spaces.SimplifyWhiteSpace(WTF::kDoNotStripWhiteSpace));

  String extra_spaces_and_newlines(" \nHello\n world\n ");
  EXPECT_EQ(String("Hello world"),
            extra_spaces_and_newlines.SimplifyWhiteSpace());
  EXPECT_EQ(
      String("  Hello  world  "),
      extra_spaces_and_newlines.SimplifyWhiteSpace(WTF::kDoNotStripWhiteSpace));

  String extra_spaces_and_tabs(" \nHello\t world\t ");
  EXPECT_EQ(String("Hello world"), extra_spaces_and_tabs.SimplifyWhiteSpace());
  EXPECT_EQ(
      String("  Hello  world  "),
      extra_spaces_and_tabs.SimplifyWhiteSpace(WTF::kDoNotStripWhiteSpace));

  auto is_space_or_g = [](UChar character) {
    return character == ' ' || character == 'G';
  };
  String extra_spaces_and_gs(" GGG Hello G world G G");
  EXPECT_EQ(String("Hello world"),
            extra_spaces_and_gs.SimplifyWhiteSpace(is_space_or_g));
  EXPECT_EQ(String("     Hello   world    "),
            extra_spaces_and_gs.SimplifyWhiteSpace(is_space_or_g,
                                                   WTF::kDoNotStripWhiteSpace));
}

TEST(StringTest, StartsWithIgnoringUnicodeCase) {
  // [U+017F U+212A i a] starts with "sk".
  EXPECT_TRUE(
      String::FromUTF8("\xC5\xBF\xE2\x84\xAAia").StartsWithIgnoringCase("sk"));
}

TEST(StringTest, StartsWithIgnoringASCIICase) {
  String all_ascii("LINK");
  String all_ascii_lower_case("link");
  EXPECT_TRUE(all_ascii.StartsWithIgnoringASCIICase(all_ascii_lower_case));
  String all_ascii_mixed_case("lInK");
  EXPECT_TRUE(all_ascii.StartsWithIgnoringASCIICase(all_ascii_mixed_case));
  String all_ascii_different("foo");
  EXPECT_FALSE(all_ascii.StartsWithIgnoringASCIICase(all_ascii_different));
  String non_ascii = String::FromUTF8("LIN\xE2\x84\xAA");
  EXPECT_FALSE(all_ascii.StartsWithIgnoringASCIICase(non_ascii));
  EXPECT_TRUE(
      all_ascii.StartsWithIgnoringASCIICase(non_ascii.DeprecatedLower()));

  EXPECT_FALSE(non_ascii.StartsWithIgnoringASCIICase(all_ascii));
  EXPECT_FALSE(non_ascii.StartsWithIgnoringASCIICase(all_ascii_lower_case));
  EXPECT_FALSE(non_ascii.StartsWithIgnoringASCIICase(all_ascii_mixed_case));
  EXPECT_FALSE(non_ascii.StartsWithIgnoringASCIICase(all_ascii_different));
}

TEST(StringTest, EndsWithIgnoringASCIICase) {
  String all_ascii("LINK");
  String all_ascii_lower_case("link");
  EXPECT_TRUE(all_ascii.EndsWithIgnoringASCIICase(all_ascii_lower_case));
  String all_ascii_mixed_case("lInK");
  EXPECT_TRUE(all_ascii.EndsWithIgnoringASCIICase(all_ascii_mixed_case));
  String all_ascii_different("foo");
  EXPECT_FALSE(all_ascii.EndsWithIgnoringASCIICase(all_ascii_different));
  String non_ascii = String::FromUTF8("LIN\xE2\x84\xAA");
  EXPECT_FALSE(all_ascii.EndsWithIgnoringASCIICase(non_ascii));
  EXPECT_TRUE(all_ascii.EndsWithIgnoringASCIICase(non_ascii.DeprecatedLower()));

  EXPECT_FALSE(non_ascii.EndsWithIgnoringASCIICase(all_ascii));
  EXPECT_FALSE(non_ascii.EndsWithIgnoringASCIICase(all_ascii_lower_case));
  EXPECT_FALSE(non_ascii.EndsWithIgnoringASCIICase(all_ascii_mixed_case));
  EXPECT_FALSE(non_ascii.EndsWithIgnoringASCIICase(all_ascii_different));
}

TEST(StringTest, EqualIgnoringASCIICase) {
  String all_ascii("LINK");
  String all_ascii_lower_case("link");
  EXPECT_TRUE(EqualIgnoringASCIICase(all_ascii, all_ascii_lower_case));
  String all_ascii_mixed_case("lInK");
  EXPECT_TRUE(EqualIgnoringASCIICase(all_ascii, all_ascii_mixed_case));
  String all_ascii_different("foo");
  EXPECT_FALSE(EqualIgnoringASCIICase(all_ascii, all_ascii_different));
  String non_ascii = String::FromUTF8("LIN\xE2\x84\xAA");
  EXPECT_FALSE(EqualIgnoringASCIICase(all_ascii, non_ascii));
  EXPECT_TRUE(EqualIgnoringASCIICase(all_ascii, non_ascii.DeprecatedLower()));

  EXPECT_FALSE(EqualIgnoringASCIICase(non_ascii, all_ascii));
  EXPECT_FALSE(EqualIgnoringASCIICase(non_ascii, all_ascii_lower_case));
  EXPECT_FALSE(EqualIgnoringASCIICase(non_ascii, all_ascii_mixed_case));
  EXPECT_FALSE(EqualIgnoringASCIICase(non_ascii, all_ascii_different));
}

TEST(StringTest, FindIgnoringASCIICase) {
  String needle = String::FromUTF8("a\xCC\x88qa\xCC\x88");

  // Multiple matches, non-overlapping
  String haystack1 = String::FromUTF8(
      "aA\xCC\x88QA\xCC\x88sA\xCC\x88qa\xCC\x88rfi\xC3\xA4q\xC3\xA4");
  EXPECT_EQ(1u, haystack1.FindIgnoringASCIICase(needle));
  EXPECT_EQ(7u, haystack1.FindIgnoringASCIICase(needle, 2));
  EXPECT_EQ(kNotFound, haystack1.FindIgnoringASCIICase(needle, 8));

  // Multiple matches, overlapping
  String haystack2 = String::FromUTF8("aA\xCC\x88QA\xCC\x88qa\xCC\x88rfi");
  EXPECT_EQ(1u, haystack2.FindIgnoringASCIICase(needle));
  EXPECT_EQ(4u, haystack2.FindIgnoringASCIICase(needle, 2));
  EXPECT_EQ(kNotFound, haystack2.FindIgnoringASCIICase(needle, 5));
}

TEST(StringTest, DeprecatedLower) {
  EXPECT_EQ("link", String("LINK").DeprecatedLower());
  EXPECT_EQ("link", String("lInk").DeprecatedLower());
  EXPECT_EQ("lin\xE1k", String("lIn\xC1k").DeprecatedLower().Latin1());

  // U+212A -> k
  EXPECT_EQ("link",
            String::FromUTF8("LIN\xE2\x84\xAA").DeprecatedLower().Utf8());
}

TEST(StringTest, Ensure16Bit) {
  String string8("8bit");
  EXPECT_TRUE(string8.Is8Bit());
  string8.Ensure16Bit();
  EXPECT_FALSE(string8.Is8Bit());
  EXPECT_EQ("8bit", string8);

  String string16(reinterpret_cast<const UChar*>(u"16bit"));
  EXPECT_FALSE(string16.Is8Bit());
  string16.Ensure16Bit();
  EXPECT_FALSE(string16.Is8Bit());
  EXPECT_EQ("16bit", string16);

  String empty8(StringImpl::empty_);
  EXPECT_TRUE(empty8.Is8Bit());
  empty8.Ensure16Bit();
  EXPECT_FALSE(empty8.Is8Bit());
  EXPECT_TRUE(empty8.empty());
  EXPECT_FALSE(empty8.IsNull());

  String empty16(StringImpl::empty16_bit_);
  EXPECT_FALSE(empty16.Is8Bit());
  empty16.Ensure16Bit();
  EXPECT_FALSE(empty16.Is8Bit());
  EXPECT_TRUE(empty16.empty());
  EXPECT_FALSE(empty16.IsNull());

  String null_string;
  null_string.Ensure16Bit();
  EXPECT_TRUE(null_string.IsNull());
}

std::string ToStdStringThroughPrinter(const String& string) {
  std::ostringstream output;
  output << string;
  return output.str();
}

TEST(StringTest, StringPrinter) {
  EXPECT_EQ("\"Hello!\"", ToStdStringThroughPrinter("Hello!"));
  EXPECT_EQ("\"\\\"\"", ToStdStringThroughPrinter("\""));
  EXPECT_EQ("\"\\\\\"", ToStdStringThroughPrinter("\\"));
  EXPECT_EQ("\"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\"",
            ToStdStringThroughPrinter(
                String("\x00\x01\x02\x03\x04\x05\x06\x07", 8u)));
  EXPECT_EQ("\"\\u0008\\t\\n\\u000B\\u000C\\r\\u000E\\u000F\"",
            ToStdStringThroughPrinter(
                String("\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 8u)));
  EXPECT_EQ("\"\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\"",
            ToStdStringThroughPrinter(
                String("\x10\x11\x12\x13\x14\x15\x16\x17", 8u)));
  EXPECT_EQ("\"\\u0018\\u0019\\u001A\\u001B\\u001C\\u001D\\u001E\\u001F\"",
            ToStdStringThroughPrinter(
                String("\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F", 8u)));
  EXPECT_EQ("\"\\u007F\\u0080\\u0081\"",
            ToStdStringThroughPrinter("\x7F\x80\x81"));
  EXPECT_EQ("\"\"", ToStdStringThroughPrinter(g_empty_string));
  EXPECT_EQ("<null>", ToStdStringThroughPrinter(String()));

  static const UChar kUnicodeSample[] = {0x30C6, 0x30B9,
                                         0x30C8};  // "Test" in Japanese.
  EXPECT_EQ("\"\\u30C6\\u30B9\\u30C8\"",
            ToStdStringThroughPrinter(
                String(kUnicodeSample, std::size(kUnicodeSample))));
}

class TestMatcher {
 public:
  explicit TestMatcher(UChar target) : target_(target) {}

  bool IsTarget(UChar ch) { return ch == target_; }

 private:
  UChar target_;
};

TEST(StringTest, FindWithCallback) {
  String test_string1("abc");
  String test_string2("stu");

  // An instance method.
  TestMatcher matcher('t');
  // Unretained is safe because callback executes synchronously in Find().
  auto callback =
      WTF::BindRepeating(&TestMatcher::IsTarget, WTF::Unretained(&matcher));
  EXPECT_EQ(WTF::kNotFound, test_string1.Find(callback));
  EXPECT_EQ(1U, test_string2.Find(callback));
}

TEST(StringTest, StartsWithIgnoringCaseAndAccents) {
  EXPECT_TRUE(String(u"ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ")
                  .StartsWithIgnoringCaseAndAccents(String("international")));
}

TEST(StringTest, StartsWithIgnoringCaseAndAccents8Bit) {
  EXPECT_TRUE(String("PuPpY").StartsWithIgnoringCaseAndAccents(String("pup")));
}

TEST(StringTest, StartsWithIgnoringCaseAndAccentsExpanding) {
  EXPECT_TRUE(
      String(u"Straße").StartsWithIgnoringCaseAndAccents(String("STRASS")));
}

TEST(StringTest, StartsWithIgnoringCaseAndAccentsSuffixDiff) {
  EXPECT_FALSE(
      String("Donkey").StartsWithIgnoringCaseAndAccents(String("Donka")));
}

}  // namespace WTF
