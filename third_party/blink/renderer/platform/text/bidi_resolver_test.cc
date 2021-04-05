/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/bidi_resolver.h"

#include <fstream>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/bidi_test_harness.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

TEST(BidiResolver, Basic) {
  bool has_strong_directionality;
  String value("foo");
  TextRun run(value);
  BidiResolver<TextRunIterator, BidiCharacterRun> bidi_resolver;
  bidi_resolver.SetStatus(
      BidiStatus(run.Direction(), run.DirectionalOverride()));
  bidi_resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));
  TextDirection direction = bidi_resolver.DetermineParagraphDirectionality(
      &has_strong_directionality);
  EXPECT_TRUE(has_strong_directionality);
  EXPECT_EQ(TextDirection::kLtr, direction);
}

TextDirection DetermineParagraphDirectionality(
    const TextRun& text_run,
    bool* has_strong_directionality = nullptr) {
  BidiResolver<TextRunIterator, BidiCharacterRun> resolver;
  resolver.SetStatus(BidiStatus(TextDirection::kLtr, false));
  resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&text_run, 0));
  return resolver.DetermineParagraphDirectionality(has_strong_directionality);
}

struct TestData {
  UChar text[3];
  size_t length;
  TextDirection expected_direction;
  bool expected_strong;
};

void TestDirectionality(const TestData& entry) {
  bool has_strong_directionality;
  String data(entry.text, entry.length);
  TextRun run(data);
  TextDirection direction =
      DetermineParagraphDirectionality(run, &has_strong_directionality);
  EXPECT_EQ(entry.expected_strong, has_strong_directionality);
  EXPECT_EQ(entry.expected_direction, direction);
}

TEST(BidiResolver, ParagraphDirectionSurrogates) {
  const TestData kTestData[] = {
      // Test strong RTL, non-BMP. (U+10858 Imperial
      // Aramaic number one, strong RTL)
      {{0xD802, 0xDC58}, 2, TextDirection::kRtl, true},

      // Test strong LTR, non-BMP. (U+1D15F Musical
      // symbol quarter note, strong LTR)
      {{0xD834, 0xDD5F}, 2, TextDirection::kLtr, true},

      // Test broken surrogate: valid leading, invalid
      // trail. (Lead of U+10858, space)
      {{0xD802, ' '}, 2, TextDirection::kLtr, false},

      // Test broken surrogate: invalid leading. (Trail
      // of U+10858, U+05D0 Hebrew Alef)
      {{0xDC58, 0x05D0}, 2, TextDirection::kRtl, true},

      // Test broken surrogate: valid leading, invalid
      // trail/valid lead, valid trail.
      {{0xD802, 0xD802, 0xDC58}, 3, TextDirection::kRtl, true},

      // Test broken surrogate: valid leading, no trail
      // (string too short). (Lead of U+10858)
      {{0xD802, 0xDC58}, 1, TextDirection::kLtr, false},

      // Test broken surrogate: trail appearing before
      // lead. (U+10858 units reversed)
      {{0xDC58, 0xD802}, 2, TextDirection::kLtr, false}};
  for (size_t i = 0; i < base::size(kTestData); ++i)
    TestDirectionality(kTestData[i]);
}

class BidiTestRunner {
  STACK_ALLOCATED();

 public:
  BidiTestRunner()
      : tests_run_(0),
        tests_skipped_(0),
        ignored_char_failures_(0),
        level_failures_(0),
        order_failures_(0) {}

  void SkipTestsWith(UChar codepoint) {
    skipped_code_points_.insert(codepoint);
  }

  void RunTest(const std::basic_string<UChar>& input,
               const Vector<int>& reorder,
               const Vector<int>& levels,
               bidi_test::ParagraphDirection,
               const std::string& line,
               size_t line_number);

  size_t tests_run_;
  size_t tests_skipped_;
  HashSet<UChar> skipped_code_points_;
  size_t ignored_char_failures_;
  size_t level_failures_;
  size_t order_failures_;
};

// Blink's UBA does not filter out control characters, etc. Maybe it should?
// Instead it depends on later layers of Blink to simply ignore them.
// This function helps us emulate that to be compatible with BidiTest.txt
// expectations.
static bool IsNonRenderedCodePoint(UChar c) {
  // The tests also expect us to ignore soft-hyphen.
  if (c == 0xAD)
    return true;
  // Control characters are not rendered:
  return c >= 0x202A && c <= 0x202E;
  // But it seems to expect LRI, etc. to be rendered!?
}

std::string DiffString(const Vector<int>& actual, const Vector<int>& expected) {
  std::ostringstream diff;
  diff << "actual: ";
  // This is the magical way to print a vector to a stream, clear, right?
  std::copy(actual.begin(), actual.end(),
            std::ostream_iterator<int>(diff, " "));
  diff << " expected: ";
  std::copy(expected.begin(), expected.end(),
            std::ostream_iterator<int>(diff, " "));
  return diff.str();
}

void BidiTestRunner::RunTest(const std::basic_string<UChar>& input,
                             const Vector<int>& expected_order,
                             const Vector<int>& expected_levels,
                             bidi_test::ParagraphDirection paragraph_direction,
                             const std::string& line,
                             size_t line_number) {
  if (!skipped_code_points_.IsEmpty()) {
    for (size_t i = 0; i < input.size(); i++) {
      if (skipped_code_points_.Contains(input[i])) {
        tests_skipped_++;
        return;
      }
    }
  }

  tests_run_++;

  TextRun text_run(input.data(), input.size());
  switch (paragraph_direction) {
    case bidi_test::kDirectionAutoLTR:
      text_run.SetDirection(DetermineParagraphDirectionality(text_run));
      break;
    case bidi_test::kDirectionLTR:
      text_run.SetDirection(TextDirection::kLtr);
      break;
    case bidi_test::kDirectionRTL:
      text_run.SetDirection(TextDirection::kRtl);
      break;
    default:
      break;
  }
  BidiResolver<TextRunIterator, BidiCharacterRun> resolver;
  resolver.SetStatus(
      BidiStatus(text_run.Direction(), text_run.DirectionalOverride()));
  resolver.SetPositionIgnoringNestedIsolates(TextRunIterator(&text_run, 0));

  BidiRunList<BidiCharacterRun>& runs = resolver.Runs();
  resolver.CreateBidiRunsForLine(TextRunIterator(&text_run, text_run.length()));

  std::ostringstream error_context;
  error_context << ", line " << line_number << " \"" << line << "\"";
  error_context << " context: "
                << bidi_test::NameFromParagraphDirection(paragraph_direction);

  Vector<int> actual_order;
  Vector<int> actual_levels;
  actual_levels.Fill(-1, input.size());
  BidiCharacterRun* run = runs.FirstRun();
  while (run) {
    // Blink's UBA just makes runs, the actual ordering of the display of
    // characters is handled later in our pipeline, so we fake it here:
    bool reversed = run->Reversed(false);
    DCHECK_GE(run->Stop(), run->Start());
    size_t length = run->Stop() - run->Start();
    for (size_t i = 0; i < length; i++) {
      int input_index = reversed ? run->Stop() - i - 1 : run->Start() + i;
      if (!IsNonRenderedCodePoint(input[input_index]))
        actual_order.push_back(input_index);
      // BidiTest.txt gives expected level data in the order of the original
      // input.
      actual_levels[input_index] = run->Level();
    }
    run = run->Next();
  }

  if (expected_order.size() != actual_order.size()) {
    ignored_char_failures_++;
    EXPECT_EQ(expected_order.size(), actual_order.size())
        << error_context.str();
  } else if (expected_order != actual_order) {
    order_failures_++;
    printf("ORDER %s%s\n", DiffString(actual_order, expected_order).c_str(),
           error_context.str().c_str());
  }

  if (expected_levels.size() != actual_levels.size()) {
    ignored_char_failures_++;
    EXPECT_EQ(expected_levels.size(), actual_levels.size())
        << error_context.str();
  } else {
    for (size_t i = 0; i < expected_levels.size(); i++) {
      // level == -1 means the level should be ignored.
      if (expected_levels[i] == actual_levels[i] || expected_levels[i] == -1)
        continue;

      printf("LEVELS %s%s\n",
             DiffString(actual_levels, expected_levels).c_str(),
             error_context.str().c_str());
      level_failures_++;
      break;
    }
  }
  runs.DeleteRuns();
}

TEST(BidiResolver, DISABLED_BidiTest_txt) {
  BidiTestRunner runner;
  // Blink's Unicode Bidi Algorithm (UBA) doesn't yet support the
  // new isolate directives from Unicode 6.3:
  // http://www.unicode.org/reports/tr9/#Explicit_Directional_Isolates
  runner.SkipTestsWith(0x2066);  // LRI
  runner.SkipTestsWith(0x2067);  // RLI
  runner.SkipTestsWith(0x2068);  // FSI
  runner.SkipTestsWith(0x2069);  // PDI

  std::string bidi_test_path = "BidiTest.txt";
  std::ifstream bidi_test_file(bidi_test_path.c_str());
  EXPECT_TRUE(bidi_test_file.is_open());
  bidi_test::Harness<BidiTestRunner> harness(runner);
  harness.Parse(bidi_test_file);
  bidi_test_file.close();

  if (runner.tests_skipped_)
    LOG(WARNING) << "WARNING: Skipped " << runner.tests_skipped_ << " tests.";
  LOG(INFO) << "Ran " << runner.tests_run_
            << " tests: " << runner.level_failures_ << " level failures "
            << runner.order_failures_ << " order failures.";

  // The unittest harness only pays attention to GTest output, so we verify
  // that the tests behaved as expected:
  EXPECT_EQ(352098u, runner.tests_run_);
  EXPECT_EQ(418143u, runner.tests_skipped_);
  EXPECT_EQ(0u, runner.ignored_char_failures_);
  EXPECT_EQ(44882u, runner.level_failures_);
  EXPECT_EQ(19151u, runner.order_failures_);
}

TEST(BidiResolver, DISABLED_BidiCharacterTest_txt) {
  BidiTestRunner runner;
  // Blink's Unicode Bidi Algorithm (UBA) doesn't yet support the
  // new isolate directives from Unicode 6.3:
  // http://www.unicode.org/reports/tr9/#Explicit_Directional_Isolates
  runner.SkipTestsWith(0x2066);  // LRI
  runner.SkipTestsWith(0x2067);  // RLI
  runner.SkipTestsWith(0x2068);  // FSI
  runner.SkipTestsWith(0x2069);  // PDI

  std::string bidi_test_path = "BidiCharacterTest.txt";
  std::ifstream bidi_test_file(bidi_test_path.c_str());
  EXPECT_TRUE(bidi_test_file.is_open());
  bidi_test::CharacterHarness<BidiTestRunner> harness(runner);
  harness.Parse(bidi_test_file);
  bidi_test_file.close();

  if (runner.tests_skipped_)
    LOG(WARNING) << "WARNING: Skipped " << runner.tests_skipped_ << " tests.";
  LOG(INFO) << "Ran " << runner.tests_run_
            << " tests: " << runner.level_failures_ << " level failures "
            << runner.order_failures_ << " order failures.";

  EXPECT_EQ(91660u, runner.tests_run_);
  EXPECT_EQ(39u, runner.tests_skipped_);
  EXPECT_EQ(0u, runner.ignored_char_failures_);
  EXPECT_EQ(14533u, runner.level_failures_);
  EXPECT_EQ(14533u, runner.order_failures_);
}

}  // namespace blink
