// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_command_line_flags.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_test.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_logging.h"

DEFINE_QUICHE_COMMAND_LINE_FLAG(bool, foo, false, "An old silent pond...");
DEFINE_QUICHE_COMMAND_LINE_FLAG(int32_t,
                                bar,
                                123,
                                "A frog jumps into the pond,");
DEFINE_QUICHE_COMMAND_LINE_FLAG(std::string, baz, "splash!", "Silence again.");

namespace quiche::test {

class QuicheCommandLineFlagTest : public QuicheTest {
 protected:
  void SetUp() override { QuicheFlagRegistry::GetInstance().ResetFlags(); }

  static QuicheParseCommandLineFlagsResult QuicheParseCommandLineFlagsForTest(
      const char* usage,
      int argc,
      const char* const* argv) {
    base::CommandLine::StringVector v;
    FillCommandLineArgs(argc, argv, &v);
    return QuicheParseCommandLineFlagsHelper(usage, base::CommandLine(v));
  }

 private:
  // Overload for platforms where base::CommandLine::StringType == std::string.
  static void FillCommandLineArgs(int argc,
                                  const char* const* argv,
                                  std::vector<std::string>* v) {
    for (int i = 0; i < argc; ++i) {
      v->push_back(argv[i]);
    }
  }

  // Overload for platforms where base::CommandLine::StringType ==
  // std::u16string.
  static void FillCommandLineArgs(int argc,
                                  const char* const* argv,
                                  std::vector<std::u16string>* v) {
    for (int i = 0; i < argc; ++i) {
      v->push_back(base::UTF8ToUTF16(argv[i]));
    }
  }
};

TEST_F(QuicheCommandLineFlagTest, DefaultValues) {
  EXPECT_EQ(false, GetQuicheFlag(foo));
  EXPECT_EQ(123, GetQuicheFlag(bar));
  EXPECT_EQ("splash!", GetQuicheFlag(baz));
}

TEST_F(QuicheCommandLineFlagTest, NotSpecified) {
  const char* argv[]{"one", "two", "three"};
  auto parse_result = QuicheParseCommandLineFlagsForTest("usage message",
                                                         std::size(argv), argv);
  EXPECT_FALSE(parse_result.exit_status.has_value());
  std::vector<std::string> expected_args{"two", "three"};
  EXPECT_EQ(expected_args, parse_result.non_flag_args);

  EXPECT_EQ(false, GetQuicheFlag(foo));
  EXPECT_EQ(123, GetQuicheFlag(bar));
  EXPECT_EQ("splash!", GetQuicheFlag(baz));
}

TEST_F(QuicheCommandLineFlagTest, BoolFlag) {
  for (const char* s :
       {"--foo", "--foo=1", "--foo=t", "--foo=True", "--foo=Y", "--foo=yes"}) {
    SetQuicheFlag(foo, false);
    const char* argv[]{"argv0", s};
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_TRUE(GetQuicheFlag(foo));
  }

  for (const char* s :
       {"--foo=0", "--foo=f", "--foo=False", "--foo=N", "--foo=no"}) {
    SetQuicheFlag(foo, true);
    const char* argv[]{"argv0", s};
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_FALSE(GetQuicheFlag(foo));
  }

  for (const char* s : {"--foo=7", "--foo=abc", "--foo=trueish"}) {
    SetQuicheFlag(foo, false);
    const char* argv[]{"argv0", s};

    testing::internal::CaptureStderr();
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    std::string captured_stderr = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(parse_result.exit_status.has_value());
    EXPECT_EQ(1, *parse_result.exit_status);
    EXPECT_THAT(captured_stderr,
                testing::ContainsRegex("Invalid value.*for flag --foo"));
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_FALSE(GetQuicheFlag(foo));
  }
}

TEST_F(QuicheCommandLineFlagTest, Int32Flag) {
  for (const int i : {-1, 0, 100, 38239832}) {
    SetQuicheFlag(bar, 0);
    std::string flag_str = base::StringPrintf("--bar=%d", i);
    const char* argv[]{"argv0", flag_str.c_str()};
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(i, GetQuicheFlag(bar));
  }

  for (const char* s : {"--bar", "--bar=a", "--bar=9999999999999"}) {
    SetQuicheFlag(bar, 0);
    const char* argv[]{"argv0", s};

    testing::internal::CaptureStderr();
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    std::string captured_stderr = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(parse_result.exit_status.has_value());
    EXPECT_EQ(1, *parse_result.exit_status);
    EXPECT_THAT(captured_stderr,
                testing::ContainsRegex("Invalid value.*for flag --bar"));
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(0, GetQuicheFlag(bar));
  }
}

TEST_F(QuicheCommandLineFlagTest, StringFlag) {
  {
    SetQuicheFlag(baz, "whee");
    const char* argv[]{"argv0", "--baz"};
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ("", GetQuicheFlag(baz));
  }

  for (const char* s : {"", "12345", "abcdefg"}) {
    SetQuicheFlag(baz, "qux");
    std::string flag_str = base::StrCat({"--baz=", s});
    const char* argv[]{"argv0", flag_str.c_str()};
    auto parse_result = QuicheParseCommandLineFlagsForTest(
        "usage message", std::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(s, GetQuicheFlag(baz));
  }
}

TEST_F(QuicheCommandLineFlagTest, PrintHelp) {
  testing::internal::CaptureStdout();
  QuichePrintCommandLineFlagHelp("usage message");
  std::string captured_stdout = testing::internal::GetCapturedStdout();
  EXPECT_THAT(captured_stdout, testing::HasSubstr("usage message"));
  EXPECT_THAT(captured_stdout,
              testing::ContainsRegex("--help +Print this help message."));
  EXPECT_THAT(captured_stdout,
              testing::ContainsRegex("--foo +An old silent pond..."));
  EXPECT_THAT(captured_stdout,
              testing::ContainsRegex("--bar +A frog jumps into the pond,"));
  EXPECT_THAT(captured_stdout, testing::ContainsRegex("--baz +Silence again."));
}

}  // namespace quiche::test
