// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_serializer.h"

#include <stdint.h>
#include <unistd.h>

#include "base/process/kill.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#define SPAWN_AND_VERIFY_PROCESS(TestName)                   \
  TEST_P(SandboxSerializerTest, TestName##Test) {            \
    base::Process process = SpawnChild(#TestName "Process"); \
    ASSERT_TRUE(process.IsValid());                          \
    int exit_code = 42;                                      \
    EXPECT_TRUE(process.WaitForExitWithTimeout(              \
        TestTimeouts::action_max_timeout(), &exit_code));    \
    EXPECT_EQ(exit_code, 0);                                 \
  }

namespace sandbox {

namespace {

constexpr char kTestParamSwitch[] = "sandbox-serializer-target";

SandboxSerializer::Target GetParamInChild() {
  std::string target_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestParamSwitch);
  CHECK(!target_str.empty());
  int target_int;
  CHECK(base::StringToInt(target_str, &target_int));
  return static_cast<SandboxSerializer::Target>(target_int);
}

}  // namespace

class SandboxSerializerTest
    : public SandboxTest,
      public testing::WithParamInterface<SandboxSerializer::Target> {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line =
        base::MultiProcessTest::MakeCmdLine(procname);
    command_line.AppendSwitchASCII(
        kTestParamSwitch, base::NumberToString(static_cast<int>(GetParam())));
    return command_line;
  }
};

MULTIPROCESS_TEST_MAIN(BasicProfileProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read* file-write* (literal "/"))
  )");

  std::string error, serialized;
  CHECK(serializer.SerializePolicy(serialized, error)) << error;
  CHECK(serializer.ApplySerializedPolicy(serialized));

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(BasicProfile)

MULTIPROCESS_TEST_MAIN(BasicProfileWithParamProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read* file-write* (literal (param "DIR")))
  )");
  CHECK(serializer.SetParameter("DIR", "/"));

  std::string error, serialized;
  CHECK(serializer.SerializePolicy(serialized, error)) << error;
  CHECK(serializer.ApplySerializedPolicy(serialized));

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(BasicProfileWithParam)

MULTIPROCESS_TEST_MAIN(ProfileFunctionalityProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read-data file-read-metadata (literal "/dev/urandom"))
  )");

  std::string error, serialized;
  CHECK(serializer.SerializePolicy(serialized, error)) << error;
  CHECK(serializer.ApplySerializedPolicy(serialized));

  // The profile compiled and applied successfully, now try and read 1 byte from
  // /dev/urandom.
  uint8_t byte;
  int fd = open("/dev/urandom", O_RDONLY);
  CHECK_NE(fd, -1);

  EXPECT_TRUE(read(fd, &byte, sizeof(byte)) == sizeof(byte));

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(ProfileFunctionality)

MULTIPROCESS_TEST_MAIN(ProfileFunctionalityWithParamsProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (if (string=? (param "ALLOW_FILE") "TRUE")
          (allow file-read-data file-read-metadata (literal (param "URANDOM")))
      )
  )");

  CHECK(serializer.SetBooleanParameter("ALLOW_FILE", true));
  CHECK(!serializer.SetParameter("ALLOW_FILE", "duplicate key is not allowed"));
  CHECK(serializer.SetParameter("URANDOM", "/dev/urandom"));

  std::string error, serialized;
  CHECK(serializer.SerializePolicy(serialized, error)) << error;
  CHECK(serializer.ApplySerializedPolicy(serialized));

  // The profile compiled and applied successfully, now try and read 1 byte from
  // /dev/urandom.
  uint8_t byte;
  int fd = open("/dev/urandom", O_RDONLY);
  CHECK_NE(fd, -1);

  EXPECT_TRUE(read(fd, &byte, sizeof(byte)) == sizeof(byte));

  // Make sure the sandbox isn't overly permissive.
  struct stat st;
  EXPECT_EQ(stat("/", &st), -1);

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(ProfileFunctionalityWithParams)

MULTIPROCESS_TEST_MAIN(ProfileFunctionalityErrorProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile("(+ 5 a)");

  // Make sure that this invalid profile results in an error returned.
  std::string error, serialized;
  CHECK(!(serializer.SerializePolicy(serialized, error) &&
          serializer.ApplySerializedPolicy(serialized)))
      << error;

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(ProfileFunctionalityError)

// Tests deserialization code error cases.
MULTIPROCESS_TEST_MAIN(DeserializationErrorProcess) {
  SandboxSerializer serializer(GetParamInChild());
  serializer.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read* file-write* (literal (param "DIR")))
    )");
  CHECK(serializer.SetParameter("DIR", "/"));
  std::string error, serialized;

  // Mode byte is malformed.
  std::string serialized_with_bad_mode = serialized;
  serialized_with_bad_mode[0] = 2;
  CHECK(!serializer.ApplySerializedPolicy(serialized_with_bad_mode));

  // Truncated by literally any amount. Sometimes this results in a
  // deserialization failure, and other times it's an apply-time failure (if
  // truncation chops off a parameter k/v pair cleanly, for example).
  for (std::string serialized_truncated = serialized;
       !serialized_truncated.empty();
       serialized_truncated.resize(serialized_truncated.size() - 1)) {
    bool expected_pass = (serialized_truncated.size() == serialized.size());

    CHECK(expected_pass ==
          serializer.ApplySerializedPolicy(serialized_truncated));
  }

  return 0;
}
SPAWN_AND_VERIFY_PROCESS(DeserializationError)

TEST_P(SandboxSerializerTest, DuplicateKeys) {
  SandboxSerializer serializer(GetParam());
  serializer.SetProfile("(version 1)(deny default)");

  EXPECT_TRUE(serializer.SetBooleanParameter("key1", true));
  EXPECT_FALSE(serializer.SetBooleanParameter("key1", false));
  EXPECT_TRUE(serializer.SetBooleanParameter("key2", false));
  EXPECT_TRUE(serializer.SetParameter("key3", "value"));
  EXPECT_FALSE(serializer.SetParameter("key3", "value"));
}

INSTANTIATE_TEST_SUITE_P(Target,
                         SandboxSerializerTest,
                         testing::Values(SandboxSerializer::Target::kSource,
                                         SandboxSerializer::Target::kCompiled));

}  // namespace sandbox
