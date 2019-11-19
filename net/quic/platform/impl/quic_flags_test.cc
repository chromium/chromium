// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/quic/platform/impl/quic_flags_impl.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(bool, foo, false, "An old silent pond...");
DEFINE_QUIC_COMMAND_LINE_FLAG(int32_t, bar, 123, "A frog jumps into the pond,");
DEFINE_QUIC_COMMAND_LINE_FLAG(std::string, baz, "splash!", "Silence again.");

namespace quic {
namespace test {

class QuicCommandLineFlagTest : public QuicTest {
 protected:
  void SetUp() override { QuicFlagRegistry::GetInstance().ResetFlags(); }

  static QuicParseCommandLineFlagsResult QuicParseCommandLineFlagsForTest(
      const char* usage,
      int argc,
      const char* const* argv) {
    base::CommandLine::StringVector v;
    FillCommandLineArgs(argc, argv, &v);
    return QuicParseCommandLineFlagsHelper(usage, base::CommandLine(v));
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
  // base::string16.
  static void FillCommandLineArgs(int argc,
                                  const char* const* argv,
                                  std::vector<base::string16>* v) {
    for (int i = 0; i < argc; ++i) {
      v->push_back(base::UTF8ToUTF16(argv[i]));
    }
  }
};

TEST_F(QuicCommandLineFlagTest, DefaultValues) {
  EXPECT_EQ(false, GetQuicFlag(FLAGS_foo));
  EXPECT_EQ(123, GetQuicFlag(FLAGS_bar));
  EXPECT_EQ("splash!", GetQuicFlag(FLAGS_baz));
}

TEST_F(QuicCommandLineFlagTest, NotSpecified) {
  const char* argv[]{"one", "two", "three"};
  auto parse_result =
      QuicParseCommandLineFlagsForTest("usage message", base::size(argv), argv);
  EXPECT_FALSE(parse_result.exit_status.has_value());
  std::vector<std::string> expected_args{"two", "three"};
  EXPECT_EQ(expected_args, parse_result.non_flag_args);

  EXPECT_EQ(false, GetQuicFlag(FLAGS_foo));
  EXPECT_EQ(123, GetQuicFlag(FLAGS_bar));
  EXPECT_EQ("splash!", GetQuicFlag(FLAGS_baz));
}

TEST_F(QuicCommandLineFlagTest, BoolFlag) {
  for (const char* s :
       {"--foo", "--foo=1", "--foo=t", "--foo=True", "--foo=Y", "--foo=yes"}) {
    SetQuicFlag(FLAGS_foo, false);
    const char* argv[]{"argv0", s};
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_TRUE(GetQuicFlag(FLAGS_foo));
  }

  for (const char* s :
       {"--foo=0", "--foo=f", "--foo=False", "--foo=N", "--foo=no"}) {
    SetQuicFlag(FLAGS_foo, true);
    const char* argv[]{"argv0", s};
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_FALSE(GetQuicFlag(FLAGS_foo));
  }

  for (const char* s : {"--foo=7", "--foo=abc", "--foo=trueish"}) {
    SetQuicFlag(FLAGS_foo, false);
    const char* argv[]{"argv0", s};

    testing::internal::CaptureStderr();
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    std::string captured_stderr = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(parse_result.exit_status.has_value());
    EXPECT_EQ(1, *parse_result.exit_status);
    EXPECT_THAT(captured_stderr,
                testing::ContainsRegex("Invalid value.*for flag --foo"));
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_FALSE(GetQuicFlag(FLAGS_foo));
  }
}

TEST_F(QuicCommandLineFlagTest, Int32Flag) {
  for (const int i : {-1, 0, 100, 38239832}) {
    SetQuicFlag(FLAGS_bar, 0);
    std::string flag_str = base::StringPrintf("--bar=%d", i);
    const char* argv[]{"argv0", flag_str.c_str()};
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(i, GetQuicFlag(FLAGS_bar));
  }

  for (const char* s : {"--bar", "--bar=a", "--bar=9999999999999"}) {
    SetQuicFlag(FLAGS_bar, 0);
    const char* argv[]{"argv0", s};

    testing::internal::CaptureStderr();
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    std::string captured_stderr = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(parse_result.exit_status.has_value());
    EXPECT_EQ(1, *parse_result.exit_status);
    EXPECT_THAT(captured_stderr,
                testing::ContainsRegex("Invalid value.*for flag --bar"));
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(0, GetQuicFlag(FLAGS_bar));
  }
}

TEST_F(QuicCommandLineFlagTest, StringFlag) {
  {
    SetQuicFlag(FLAGS_baz, "whee");
    const char* argv[]{"argv0", "--baz"};
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ("", GetQuicFlag(FLAGS_baz));
  }

  for (const char* s : {"", "12345", "abcdefg"}) {
    SetQuicFlag(FLAGS_baz, "qux");
    std::string flag_str = base::StrCat({"--baz=", s});
    const char* argv[]{"argv0", flag_str.c_str()};
    auto parse_result = QuicParseCommandLineFlagsForTest(
        "usage message", base::size(argv), argv);
    EXPECT_FALSE(parse_result.exit_status.has_value());
    EXPECT_TRUE(parse_result.non_flag_args.empty());
    EXPECT_EQ(s, GetQuicFlag(FLAGS_baz));
  }
}

TEST_F(QuicCommandLineFlagTest, PrintHelp) {
  testing::internal::CaptureStdout();
  QuicPrintCommandLineFlagHelp("usage message");
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

class QuicFlagsTest : public QuicTest {};

TEST_F(QuicFlagsTest, SetQuicFlagByName_bool) {
  FLAGS_quic_enforce_single_packet_chlo = true;
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "false");
  EXPECT_FALSE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "true");
  EXPECT_TRUE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "False");
  EXPECT_FALSE(FLAGS_quic_enforce_single_packet_chlo);
  SetQuicFlagByName("FLAGS_quic_enforce_single_packet_chlo", "True");
  EXPECT_TRUE(FLAGS_quic_enforce_single_packet_chlo);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_double) {
  FLAGS_quic_bbr_cwnd_gain = 3.0;
  SetQuicFlagByName("FLAGS_quic_bbr_cwnd_gain", "1.5");
  EXPECT_EQ(1.5, FLAGS_quic_bbr_cwnd_gain);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_double_invalid) {
  FLAGS_quic_bbr_cwnd_gain = 3.0;
  SetQuicFlagByName("FLAGS_quic_bbr_cwnd_gain", "true");
  EXPECT_EQ(3.0, FLAGS_quic_bbr_cwnd_gain);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_int64_t) {
  FLAGS_quic_max_tracked_packet_count = 100;
  SetQuicFlagByName("FLAGS_quic_max_tracked_packet_count", "5");
  EXPECT_EQ(5, FLAGS_quic_max_tracked_packet_count);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_int64_t_invalid) {
  FLAGS_quic_max_tracked_packet_count = 100;
  SetQuicFlagByName("FLAGS_quic_max_tracked_packet_count", "false");
  EXPECT_EQ(100, FLAGS_quic_max_tracked_packet_count);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_uint32_t) {
  FLAGS_quic_send_buffer_max_data_slice_size = 4096;
  SetQuicFlagByName("FLAGS_quic_send_buffer_max_data_slice_size", "1024");
  EXPECT_EQ(1024u, FLAGS_quic_send_buffer_max_data_slice_size);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_uint32_t_invalid) {
  FLAGS_quic_send_buffer_max_data_slice_size = 4096;
  SetQuicFlagByName("FLAGS_quic_send_buffer_max_data_slice_size", "false");
  EXPECT_EQ(4096u, FLAGS_quic_send_buffer_max_data_slice_size);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_uint32_t_negative) {
  FLAGS_quic_send_buffer_max_data_slice_size = 4096;
  SetQuicFlagByName("FLAGS_quic_send_buffer_max_data_slice_size", "-1");
  EXPECT_EQ(4096u, FLAGS_quic_send_buffer_max_data_slice_size);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_uint32_t_too_large) {
  FLAGS_quic_send_buffer_max_data_slice_size = 4096;
  SetQuicFlagByName("FLAGS_quic_send_buffer_max_data_slice_size", "4294967297");
  EXPECT_EQ(4096u, FLAGS_quic_send_buffer_max_data_slice_size);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_int32_t) {
  FLAGS_quic_lumpy_pacing_size = 1;
  SetQuicFlagByName("FLAGS_quic_lumpy_pacing_size", "10");
  EXPECT_EQ(10, FLAGS_quic_lumpy_pacing_size);
}

TEST_F(QuicFlagsTest, SetQuicFlagByName_int32_t_invalid) {
  FLAGS_quic_lumpy_pacing_size = 1;
  SetQuicFlagByName("FLAGS_quic_lumpy_pacing_size", "false");
  EXPECT_EQ(1, FLAGS_quic_lumpy_pacing_size);
}

}  // namespace test
}  // namespace quic
