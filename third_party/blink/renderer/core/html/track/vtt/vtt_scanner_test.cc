/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(VTTScannerTest, Constructor) {
  test::TaskEnvironment task_environment;
  String data8("foo");
  EXPECT_TRUE(data8.Is8Bit());
  VTTScanner scanner8(data8);
  EXPECT_FALSE(scanner8.IsAtEnd());

  String data16(data8);
  data16.Ensure16Bit();
  EXPECT_FALSE(data16.Is8Bit());
  VTTScanner scanner16(data16);
  EXPECT_FALSE(scanner16.IsAtEnd());

  VTTScanner scanner_empty(g_empty_string);
  EXPECT_TRUE(scanner_empty.IsAtEnd());
}

void ScanSequenceHelper1(const String& input) {
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  EXPECT_TRUE(scanner.Match('f'));
  EXPECT_FALSE(scanner.Match('o'));

  EXPECT_TRUE(scanner.Scan('f'));
  EXPECT_FALSE(scanner.Match('f'));
  EXPECT_TRUE(scanner.Match('o'));

  EXPECT_FALSE(scanner.Scan('e'));
  EXPECT_TRUE(scanner.Scan('o'));

  EXPECT_TRUE(scanner.Scan('e'));
  EXPECT_FALSE(scanner.Match('e'));

  EXPECT_TRUE(scanner.IsAtEnd());
}

// Run TESTFUNC with DATA in Latin and then UTF-16. (Requires DATA being Latin.)
#define TEST_WITH(TESTFUNC, DATA)  \
  do {                             \
    String data8(DATA);            \
    EXPECT_TRUE(data8.Is8Bit());   \
    TESTFUNC(data8);               \
                                   \
    String data16(data8);          \
    data16.Ensure16Bit();          \
    EXPECT_FALSE(data16.Is8Bit()); \
    TESTFUNC(data16);              \
  } while (false)

// Exercises match(c) and scan(c).
TEST(VTTScannerTest, BasicOperations1) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanSequenceHelper1, "foe");
}

void ScanSequenceHelper2(const String& input) {
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  EXPECT_FALSE(scanner.Scan("fe"));

  EXPECT_TRUE(scanner.Scan("fo"));
  EXPECT_FALSE(scanner.IsAtEnd());

  EXPECT_FALSE(scanner.Scan("ee"));

  EXPECT_TRUE(scanner.Scan('e'));
  EXPECT_TRUE(scanner.IsAtEnd());
}

// Exercises scan(<literal>[, length]).
TEST(VTTScannerTest, BasicOperations2) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanSequenceHelper2, "foe");
}

bool LowerCaseAlpha(UChar c) {
  return c >= 'a' && c <= 'z';
}

void ScanWithPredicate(const String& input) {
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  // Collect "bad".
  VTTScanner::Run lc_run = scanner.CollectWhile<LowerCaseAlpha>();
  // collectWhile doesn't move the scan position.
  EXPECT_TRUE(scanner.Match('b'));
  // Consume "bad".
  scanner.SkipWhile<LowerCaseAlpha>();
  EXPECT_TRUE(scanner.Match('A'));
  EXPECT_TRUE(scanner.IsAt(lc_run.end()));

  // Consume "A".
  EXPECT_TRUE(scanner.Scan('A'));

  // Collect "bing".
  lc_run = scanner.CollectWhile<LowerCaseAlpha>();
  // collectWhile doesn't move the scan position.
  EXPECT_FALSE(scanner.IsAtEnd());
  // Consume "bing".
  scanner.SkipWhile<LowerCaseAlpha>();
  EXPECT_TRUE(scanner.IsAt(lc_run.end()));
  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests skipWhile() and collectWhile().
TEST(VTTScannerTest, PredicateScanning) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanWithPredicate, "badAbing");
}

void ScanWithInvPredicate(const String& input) {
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  // Collect "BAD".
  VTTScanner::Run uc_run = scanner.CollectUntil<LowerCaseAlpha>();
  // collectUntil doesn't move the scan position.
  EXPECT_TRUE(scanner.Match('B'));
  // Consume "BAD".
  scanner.SkipUntil<LowerCaseAlpha>();
  EXPECT_TRUE(scanner.Match('a'));
  EXPECT_TRUE(scanner.IsAt(uc_run.end()));

  // Consume "a".
  EXPECT_TRUE(scanner.Scan('a'));

  // Collect "BING".
  uc_run = scanner.CollectUntil<LowerCaseAlpha>();
  // collectUntil doesn't move the scan position.
  EXPECT_FALSE(scanner.IsAtEnd());
  // Consume "BING".
  scanner.SkipUntil<LowerCaseAlpha>();
  EXPECT_TRUE(scanner.IsAt(uc_run.end()));
  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests skipUntil() and collectUntil().
TEST(VTTScannerTest, InversePredicateScanning) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanWithInvPredicate, "BADaBING");
}

void ScanRuns(const String& input) {
  String foo_string("foo");
  String bar_string("bar");
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  VTTScanner::Run word = scanner.CollectWhile<LowerCaseAlpha>();
  EXPECT_FALSE(scanner.ScanRun(word, bar_string));
  EXPECT_TRUE(scanner.ScanRun(word, foo_string));

  EXPECT_TRUE(scanner.Match(':'));
  EXPECT_TRUE(scanner.Scan(':'));

  // Skip 'baz'.
  scanner.SkipRun(scanner.CollectWhile<LowerCaseAlpha>());

  EXPECT_TRUE(scanner.Match(':'));
  EXPECT_TRUE(scanner.Scan(':'));

  word = scanner.CollectWhile<LowerCaseAlpha>();
  EXPECT_FALSE(scanner.ScanRun(word, foo_string));
  EXPECT_TRUE(scanner.ScanRun(word, bar_string));
  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests scanRun/skipRun.
TEST(VTTScannerTest, RunScanning) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanRuns, "foo:baz:bar");
}

void ScanRunsToStrings(const String& input) {
  VTTScanner scanner(input);
  EXPECT_FALSE(scanner.IsAtEnd());
  VTTScanner::Run word = scanner.CollectWhile<LowerCaseAlpha>();
  String foo_string = scanner.ExtractString(word);
  EXPECT_EQ(foo_string, "foo");
  EXPECT_TRUE(scanner.IsAt(word.end()));

  EXPECT_TRUE(scanner.Match(':'));
  EXPECT_TRUE(scanner.Scan(':'));

  word = scanner.CollectWhile<LowerCaseAlpha>();
  String bar_string = scanner.ExtractString(word);
  EXPECT_EQ(bar_string, "bar");
  EXPECT_TRUE(scanner.IsAt(word.end()));
  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests extractString.
TEST(VTTScannerTest, ExtractString) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanRunsToStrings, "foo:bar");
}

void TailStringExtract(const String& input) {
  VTTScanner scanner(input);
  EXPECT_TRUE(scanner.Scan("foo"));
  EXPECT_TRUE(scanner.Scan(':'));
  String bar_suffix = scanner.RestOfInputAsString();
  EXPECT_EQ(bar_suffix, "bar");

  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests restOfInputAsString().
TEST(VTTScannerTest, ExtractRestAsString) {
  test::TaskEnvironment task_environment;
  TEST_WITH(TailStringExtract, "foo:bar");
}

void ScanDigits1(const String& input) {
  VTTScanner scanner(input);
  EXPECT_TRUE(scanner.Scan("foo"));
  unsigned number;

  EXPECT_EQ(scanner.ScanDigits(number), 0u);
  EXPECT_EQ(number, 0u);

  EXPECT_TRUE(scanner.Scan(' '));
  EXPECT_EQ(scanner.ScanDigits(number), 3u);
  EXPECT_TRUE(scanner.Match(' '));
  EXPECT_EQ(number, 123u);

  EXPECT_TRUE(scanner.Scan(' '));
  EXPECT_TRUE(scanner.Scan("bar"));
  EXPECT_TRUE(scanner.Scan(' '));

  EXPECT_EQ(scanner.ScanDigits(number), 5u);
  EXPECT_EQ(number, 45678u);

  EXPECT_TRUE(scanner.IsAtEnd());
}

void ScanDigits2(const String& input) {
  VTTScanner scanner(input);
  unsigned number;
  EXPECT_EQ(scanner.ScanDigits(number), 0u);
  EXPECT_EQ(number, 0u);
  EXPECT_TRUE(scanner.Scan('-'));
  EXPECT_EQ(scanner.ScanDigits(number), 3u);
  EXPECT_EQ(number, 654u);

  EXPECT_TRUE(scanner.Scan(' '));

  EXPECT_EQ(scanner.ScanDigits(number), 19u);
  EXPECT_EQ(number, std::numeric_limits<unsigned>::max());

  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests scanDigits().
TEST(VTTScannerTest, ScanDigits) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanDigits1, "foo 123 bar 45678");
  TEST_WITH(ScanDigits2, "-654 1000000000000000000");
}

void ScanDoubleValue(const String& input) {
  VTTScanner scanner(input);
  double value;
  // "1."
  EXPECT_TRUE(scanner.ScanDouble(value));
  EXPECT_EQ(value, 1.0);
  EXPECT_TRUE(scanner.Scan(' '));

  // "1.0"
  EXPECT_TRUE(scanner.ScanDouble(value));
  EXPECT_EQ(value, 1.0);
  EXPECT_TRUE(scanner.Scan(' '));

  // ".0"
  EXPECT_TRUE(scanner.ScanDouble(value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(scanner.Scan(' '));

  // "." (invalid)
  EXPECT_FALSE(scanner.ScanDouble(value));
  EXPECT_TRUE(scanner.Match('.'));
  EXPECT_TRUE(scanner.Scan('.'));
  EXPECT_TRUE(scanner.Scan(' '));

  // "1.0000"
  EXPECT_TRUE(scanner.ScanDouble(value));
  EXPECT_EQ(value, 1.0);
  EXPECT_TRUE(scanner.Scan(' '));

  // "01.000"
  EXPECT_TRUE(scanner.ScanDouble(value));
  EXPECT_EQ(value, 1.0);

  EXPECT_TRUE(scanner.IsAtEnd());
}

// Tests ScanDouble().
TEST(VTTScannerTest, ScanDouble) {
  test::TaskEnvironment task_environment;
  TEST_WITH(ScanDoubleValue, "1. 1.0 .0 . 1.0000 01.000");
}

#undef TEST_WITH

}  // namespace blink
