// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "gpu/config/gpu_test_expectations_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

struct TestOSEntry {
  const char* name;
  GPUTestConfig::OS os;
};

static const TestOSEntry kOsFamilyWin = { "WIN", GPUTestConfig::kOsWin };
static const TestOSEntry kOsFamilyMac = { "MAC", GPUTestConfig::kOsMac };

static const struct TestOsWithFamily {
  TestOSEntry version;
  TestOSEntry family;
} kOSVersionsWithFamily[] = {
    {{"SNOWLEOPARD", GPUTestConfig::kOsMacSnowLeopard}, kOsFamilyMac},
    {{"LION", GPUTestConfig::kOsMacLion}, kOsFamilyMac},
    {{"MOUNTAINLION", GPUTestConfig::kOsMacMountainLion}, kOsFamilyMac},
    {{"MAVERICKS", GPUTestConfig::kOsMacMavericks}, kOsFamilyMac},
    {{"YOSEMITE", GPUTestConfig::kOsMacYosemite}, kOsFamilyMac},
    {{"ELCAPITAN", GPUTestConfig::kOsMacElCapitan}, kOsFamilyMac},
    {{"SIERRA", GPUTestConfig::kOsMacSierra}, kOsFamilyMac},
    {{"HIGHSIERRA", GPUTestConfig::kOsMacHighSierra}, kOsFamilyMac},
    {{"MOJAVE", GPUTestConfig::kOsMacMojave}, kOsFamilyMac},
    {{"CATALINA", GPUTestConfig::kOsMacCatalina}, kOsFamilyMac},
    {{"BIGSUR", GPUTestConfig::kOsMacBigSur}, kOsFamilyMac},
    {{"MONTEREY", GPUTestConfig::kOsMacMonterey}, kOsFamilyMac},
    {{"VENTURA", GPUTestConfig::kOsMacVentura}, kOsFamilyMac},
    {{"SONOMA", GPUTestConfig::kOsMacSonoma}, kOsFamilyMac},
    {{"SEQUOIA", GPUTestConfig::kOsMacSequoia}, kOsFamilyMac},
    {{"LINUX", GPUTestConfig::kOsLinux}, {"LINUX", GPUTestConfig::kOsLinux}},
    {{"CHROMEOS", GPUTestConfig::kOsChromeOS},
     {"CHROMEOS", GPUTestConfig::kOsChromeOS}},
    {{"ANDROID", GPUTestConfig::kOsAndroid},
     {"ANDROID", GPUTestConfig::kOsAndroid}}};

TestOSEntry GetUnrelatedOS(const TestOSEntry& os) {
  return (os.os & kOsFamilyWin.os) ? kOsFamilyMac : kOsFamilyWin;
}

// Prints test parameter details.
std::ostream& operator << (std::ostream& out, const TestOsWithFamily& os) {
  out << "{ os_name: \"" << os.version.name
      << "\", os_flag: " << os.version.os
      << ", os_family: \"" << os.family.name
      << "\", os_family_flag: " << os.family.os
      << " }";
  return out;
}

class GPUTestExpectationsParserTest : public testing::Test {
 public:
  GPUTestExpectationsParserTest() = default;

  ~GPUTestExpectationsParserTest() override = default;

  const GPUTestBotConfig& bot_config() const {
    return bot_config_;
  }

 protected:
  void SetUp() override {
    bot_config_.set_os(GPUTestConfig::kOsWin10);
    bot_config_.set_build_type(GPUTestConfig::kBuildTypeRelease);
    bot_config_.AddGPUVendor(0x10de);
    bot_config_.set_gpu_device_id(0x0640);
    bot_config_.set_api(GPUTestConfig::kAPID3D11);
    bot_config_.set_command_decoder(GPUTestConfig::kCommandDecoderPassthrough);
    ASSERT_TRUE(bot_config_.IsValid());
  }

  void TearDown() override {}

  void set_os(int32_t os) {
    bot_config_.set_os(os);
    ASSERT_TRUE(bot_config_.IsValid());
  }

 private:
  GPUTestBotConfig bot_config_;
};

class GPUTestExpectationsParserParamTest
    : public GPUTestExpectationsParserTest,
      public testing::WithParamInterface<TestOsWithFamily> {
 public:
  GPUTestExpectationsParserParamTest() = default;

  ~GPUTestExpectationsParserParamTest() override = default;

 protected:
  const GPUTestBotConfig& GetBotConfig() {
    set_os(GetParam().version.os);
    return bot_config();
  }

 private:
  // Restrict access to bot_config() function.
  // GetBotConfig() should be used instead.
  using GPUTestExpectationsParserTest::bot_config;
};

TEST_F(GPUTestExpectationsParserTest, CommentOnly) {
  const std::string text =
      "  \n"
      "// This is just some comment\n"
      "";
  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass,
            parser.GetTestExpectation("some_test", bot_config()));
}

TEST_P(GPUTestExpectationsParserParamTest, ValidFullEntry) {
  const std::string text =
      base::StringPrintf("BUG12345 %s RELEASE NVIDIA 0x0640 : MyTest = FAIL",
                         GetParam().version.name);

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestFail,
            parser.GetTestExpectation("MyTest", GetBotConfig()));
}

TEST_P(GPUTestExpectationsParserParamTest, ValidPartialEntry) {
  const std::string text =
      base::StringPrintf("BUG12345 %s NVIDIA : MyTest = TIMEOUT",
                         GetParam().family.name);

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestTimeout,
            parser.GetTestExpectation("MyTest", GetBotConfig()));
}

TEST_P(GPUTestExpectationsParserParamTest, ValidUnrelatedOsEntry) {
  const std::string text = base::StringPrintf(
      "BUG12345 %s : MyTest = TIMEOUT",
      GetUnrelatedOS(GetParam().version).name);

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass,
            parser.GetTestExpectation("MyTest", GetBotConfig()));
}

TEST_F(GPUTestExpectationsParserTest, ValidUnrelatedTestEntry) {
  const std::string text =
      "BUG12345 WIN10 RELEASE NVIDIA 0x0640 : AnotherTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, AllModifiers) {
  const std::string text =
      "BUG12345 WIN10 SEQUOIA SNOWLEOPARD LION MOUNTAINLION "
      "MAVERICKS LINUX CHROMEOS ANDROID "
      "NVIDIA INTEL AMD VMWARE RELEASE DEBUG : MyTest = "
      "PASS FAIL FLAKY TIMEOUT SKIP";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass |
            GPUTestExpectationsParser::kGpuTestFail |
            GPUTestExpectationsParser::kGpuTestFlaky |
            GPUTestExpectationsParser::kGpuTestTimeout |
            GPUTestExpectationsParser::kGpuTestSkip,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_P(GPUTestExpectationsParserParamTest, DuplicateModifiers) {
  const std::string text = base::StringPrintf(
      "BUG12345 %s %s RELEASE NVIDIA 0x0640 : MyTest = FAIL",
      GetParam().version.name,
      GetParam().version.name);

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, AllModifiersLowerCase) {
  const std::string text =
      "BUG12345 win10 sequoia snowleopard lion linux "
      "chromeos android nvidia intel amd vmware release debug : MyTest = "
      "pass fail flaky timeout skip";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass |
            GPUTestExpectationsParser::kGpuTestFail |
            GPUTestExpectationsParser::kGpuTestFlaky |
            GPUTestExpectationsParser::kGpuTestTimeout |
            GPUTestExpectationsParser::kGpuTestSkip,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, MissingColon) {
  const std::string text =
      "BUG12345 XP MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, MissingEqual) {
  const std::string text =
      "BUG12345 XP : MyTest FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, IllegalModifier) {
  const std::string text =
      "BUG12345 XP XXX : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_P(GPUTestExpectationsParserParamTest, OsConflicts) {
  const std::string text = base::StringPrintf("BUG12345 %s %s : MyTest = FAIL",
                                              GetParam().version.name,
                                              GetParam().family.name);

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, InvalidModifierCombination) {
  const std::string text =
      "BUG12345 XP NVIDIA INTEL 0x0640 : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, BadGpuDeviceID) {
  const std::string text =
      "BUG12345 XP NVIDIA 0xU07X : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, MoreThanOneGpuDeviceID) {
  const std::string text =
      "BUG12345 XP NVIDIA 0x0640 0x0641 : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_P(GPUTestExpectationsParserParamTest, MultipleEntriesConflicts) {
  const std::string text = base::StringPrintf(
      "BUG12345 %s RELEASE NVIDIA 0x0640 : MyTest = FAIL\n"
      "BUG12345 %s : MyTest = FAIL",
      GetParam().version.name,
      GetParam().family.name);

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, MultipleTests) {
  const std::string text =
      "BUG12345 WIN10 RELEASE NVIDIA 0x0640 : MyTest = FAIL\n"
      "BUG12345 WIN : AnotherTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, ValidMultipleEntries) {
  const std::string text =
      "BUG12345 WIN10 RELEASE NVIDIA 0x0640 : MyTest = FAIL\n"
      "BUG12345 LINUX : MyTest = TIMEOUT";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestFail,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, StarMatching) {
  const std::string text =
      "BUG12345 WIN10 RELEASE NVIDIA 0x0640 : MyTest* = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestFail,
            parser.GetTestExpectation("MyTest0", bot_config()));
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass,
            parser.GetTestExpectation("OtherTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, ValidAPI) {
  const std::string text =
      "BUG12345 WIN10 NVIDIA D3D9 D3D11 OPENGL GLES : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestFail,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, MultipleAPIsConflict) {
  const std::string text = "BUG12345 WIN10 NVIDIA D3D9 D3D9 : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

TEST_F(GPUTestExpectationsParserTest, PassthroughCommandDecoder) {
  const std::string text = "BUG12345 PASSTHROUGH : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestFail,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, ValidatingCommandDecoder) {
  const std::string text = "BUG12345 VALIDATING : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_TRUE(parser.LoadTestExpectations(text));
  EXPECT_EQ(0u, parser.GetErrorMessages().size());
  EXPECT_EQ(GPUTestExpectationsParser::kGpuTestPass,
            parser.GetTestExpectation("MyTest", bot_config()));
}

TEST_F(GPUTestExpectationsParserTest, MultipleCommandDecodersConflict) {
  const std::string text = "BUG12345 VALIDATING VALIDATING : MyTest = FAIL";

  GPUTestExpectationsParser parser;
  EXPECT_FALSE(parser.LoadTestExpectations(text));
  EXPECT_NE(0u, parser.GetErrorMessages().size());
}

INSTANTIATE_TEST_SUITE_P(GPUTestExpectationsParser,
                         GPUTestExpectationsParserParamTest,
                         ::testing::ValuesIn(kOSVersionsWithFamily));

}  // namespace gpu
