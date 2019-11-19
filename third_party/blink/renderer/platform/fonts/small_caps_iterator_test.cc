// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/small_caps_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

struct SmallCapsTestRun {
  const char* const text;
  SmallCapsIterator::SmallCapsBehavior code;
};

struct SmallCapsExpectedRun {
  unsigned limit;
  SmallCapsIterator::SmallCapsBehavior small_caps_behavior;

  SmallCapsExpectedRun(
      unsigned the_limit,
      SmallCapsIterator::SmallCapsBehavior the_small_caps_behavior)
      : limit(the_limit), small_caps_behavior(the_small_caps_behavior) {}
};

class SmallCapsIteratorTest : public testing::Test {
 protected:
  void CheckRuns(const Vector<SmallCapsTestRun>& runs) {
    StringBuilder text;
    text.Ensure16Bit();
    Vector<SmallCapsExpectedRun> expect;
    for (auto& run : runs) {
      text.Append(String::FromUTF8(run.text));
      expect.push_back(SmallCapsExpectedRun(text.length(), run.code));
    }
    SmallCapsIterator small_caps_iterator(text.Characters16(), text.length());
    VerifyRuns(&small_caps_iterator, expect);
  }

  void VerifyRuns(SmallCapsIterator* small_caps_iterator,
                  const Vector<SmallCapsExpectedRun>& expect) {
    unsigned limit;
    SmallCapsIterator::SmallCapsBehavior small_caps_behavior;
    size_t run_count = 0;
    while (small_caps_iterator->Consume(&limit, &small_caps_behavior)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].limit, limit);
      ASSERT_EQ(expect[run_count].small_caps_behavior, small_caps_behavior);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_SMALL_CAPS_RUNSVECTOR(...)                  \
  static const SmallCapsTestRun kRunsArray[] = __VA_ARGS__; \
  Vector<SmallCapsTestRun> runs;                            \
  runs.Append(kRunsArray, sizeof(kRunsArray) / sizeof(*kRunsArray));

#define CHECK_SMALL_CAPS_RUN(...)             \
  DECLARE_SMALL_CAPS_RUNSVECTOR(__VA_ARGS__); \
  CheckRuns(runs);

TEST_F(SmallCapsIteratorTest, Empty) {
  String empty(g_empty_string16_bit);
  SmallCapsIterator small_caps_iterator(empty.Characters16(), empty.length());
  unsigned limit = 0;
  SmallCapsIterator::SmallCapsBehavior small_caps_behavior =
      SmallCapsIterator::kSmallCapsInvalid;
  DCHECK(!small_caps_iterator.Consume(&limit, &small_caps_behavior));
  ASSERT_EQ(limit, 0u);
  ASSERT_EQ(small_caps_behavior, SmallCapsIterator::kSmallCapsInvalid);
}

TEST_F(SmallCapsIteratorTest, UppercaseA) {
  CHECK_SMALL_CAPS_RUN({{"A", SmallCapsIterator::kSmallCapsSameCase}});
}

TEST_F(SmallCapsIteratorTest, LowercaseA) {
  CHECK_SMALL_CAPS_RUN({{"a", SmallCapsIterator::kSmallCapsUppercaseNeeded}});
}

TEST_F(SmallCapsIteratorTest, UppercaseLowercaseA) {
  CHECK_SMALL_CAPS_RUN({{"A", SmallCapsIterator::kSmallCapsSameCase},
                        {"a", SmallCapsIterator::kSmallCapsUppercaseNeeded}});
}

TEST_F(SmallCapsIteratorTest, UppercasePunctuationMixed) {
  CHECK_SMALL_CAPS_RUN({{"AAA??", SmallCapsIterator::kSmallCapsSameCase}});
}

TEST_F(SmallCapsIteratorTest, LowercasePunctuationMixed) {
  CHECK_SMALL_CAPS_RUN({{"aaa", SmallCapsIterator::kSmallCapsUppercaseNeeded},
                        {"===", SmallCapsIterator::kSmallCapsSameCase}});
}

TEST_F(SmallCapsIteratorTest, LowercasePunctuationInterleaved) {
  CHECK_SMALL_CAPS_RUN({{"aaa", SmallCapsIterator::kSmallCapsUppercaseNeeded},
                        {"===", SmallCapsIterator::kSmallCapsSameCase},
                        {"bbb", SmallCapsIterator::kSmallCapsUppercaseNeeded}});
}

TEST_F(SmallCapsIteratorTest, Japanese) {
  CHECK_SMALL_CAPS_RUN({{"ほへと", SmallCapsIterator::kSmallCapsSameCase}});
}

TEST_F(SmallCapsIteratorTest, Armenian) {
  CHECK_SMALL_CAPS_RUN({{"աբգդ", SmallCapsIterator::kSmallCapsUppercaseNeeded},
                        {"ԵԶԷԸ", SmallCapsIterator::kSmallCapsSameCase}});
}

TEST_F(SmallCapsIteratorTest, CombiningCharacterSequence) {
  CHECK_SMALL_CAPS_RUN({{"èü", SmallCapsIterator::kSmallCapsUppercaseNeeded}});
}

}  // namespace blink
