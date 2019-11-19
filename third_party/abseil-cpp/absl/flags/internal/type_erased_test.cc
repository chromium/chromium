//
//  Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/flags/internal/type_erased.h"

#include <cmath>

#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"

ABSL_FLAG(int, int_flag, 1, "int_flag help");
ABSL_FLAG(std::string, string_flag, "dflt", "string_flag help");
ABSL_RETIRED_FLAG(bool, bool_retired_flag, false, "bool_retired_flag help");

namespace {

namespace flags = absl::flags_internal;

class TypeErasedTest : public testing::Test {
 protected:
  void SetUp() override { flag_saver_ = absl::make_unique<flags::FlagSaver>(); }
  void TearDown() override { flag_saver_.reset(); }

 private:
  std::unique_ptr<flags::FlagSaver> flag_saver_;
};

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestGetCommandLineOption) {
  std::string value;
  EXPECT_TRUE(flags::GetCommandLineOption("int_flag", &value));
  EXPECT_EQ(value, "1");

  EXPECT_TRUE(flags::GetCommandLineOption("string_flag", &value));
  EXPECT_EQ(value, "dflt");

  EXPECT_FALSE(flags::GetCommandLineOption("bool_retired_flag", &value));

  EXPECT_FALSE(flags::GetCommandLineOption("unknown_flag", &value));
}

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestSetCommandLineOption) {
  EXPECT_TRUE(flags::SetCommandLineOption("int_flag", "101"));
  EXPECT_EQ(absl::GetFlag(FLAGS_int_flag), 101);

  EXPECT_TRUE(flags::SetCommandLineOption("string_flag", "asdfgh"));
  EXPECT_EQ(absl::GetFlag(FLAGS_string_flag), "asdfgh");

  EXPECT_FALSE(flags::SetCommandLineOption("bool_retired_flag", "true"));

  EXPECT_FALSE(flags::SetCommandLineOption("unknown_flag", "true"));
}

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestSetCommandLineOptionWithMode_SET_FLAGS_VALUE) {
  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("int_flag", "101",
                                                  flags::SET_FLAGS_VALUE));
  EXPECT_EQ(absl::GetFlag(FLAGS_int_flag), 101);

  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("string_flag", "asdfgh",
                                                  flags::SET_FLAGS_VALUE));
  EXPECT_EQ(absl::GetFlag(FLAGS_string_flag), "asdfgh");

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("bool_retired_flag", "true",
                                                   flags::SET_FLAGS_VALUE));

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("unknown_flag", "true",
                                                   flags::SET_FLAGS_VALUE));
}

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestSetCommandLineOptionWithMode_SET_FLAG_IF_DEFAULT) {
  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("int_flag", "101",
                                                  flags::SET_FLAG_IF_DEFAULT));
  EXPECT_EQ(absl::GetFlag(FLAGS_int_flag), 101);

  // This semantic is broken. We return true instead of false. Value is not
  // updated.
  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("int_flag", "202",
                                                  flags::SET_FLAG_IF_DEFAULT));
  EXPECT_EQ(absl::GetFlag(FLAGS_int_flag), 101);

  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("string_flag", "asdfgh",
                                                  flags::SET_FLAG_IF_DEFAULT));
  EXPECT_EQ(absl::GetFlag(FLAGS_string_flag), "asdfgh");

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("bool_retired_flag", "true",
                                                   flags::SET_FLAG_IF_DEFAULT));

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("unknown_flag", "true",
                                                   flags::SET_FLAG_IF_DEFAULT));
}

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestSetCommandLineOptionWithMode_SET_FLAGS_DEFAULT) {
  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("int_flag", "101",
                                                  flags::SET_FLAGS_DEFAULT));

  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("string_flag", "asdfgh",
                                                  flags::SET_FLAGS_DEFAULT));
  EXPECT_EQ(absl::GetFlag(FLAGS_string_flag), "asdfgh");

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("bool_retired_flag", "true",
                                                   flags::SET_FLAGS_DEFAULT));

  EXPECT_FALSE(flags::SetCommandLineOptionWithMode("unknown_flag", "true",
                                                   flags::SET_FLAGS_DEFAULT));

  // This should be successfull, since flag is still is not set
  EXPECT_TRUE(flags::SetCommandLineOptionWithMode("int_flag", "202",
                                                  flags::SET_FLAG_IF_DEFAULT));
  EXPECT_EQ(absl::GetFlag(FLAGS_int_flag), 202);
}

// --------------------------------------------------------------------

TEST_F(TypeErasedTest, TestIsValidFlagValue) {
  EXPECT_TRUE(flags::IsValidFlagValue("int_flag", "57"));
  EXPECT_TRUE(flags::IsValidFlagValue("int_flag", "-101"));
  EXPECT_FALSE(flags::IsValidFlagValue("int_flag", "1.1"));

  EXPECT_TRUE(flags::IsValidFlagValue("string_flag", "#%^#%^$%DGHDG$W%adsf"));

  EXPECT_TRUE(flags::IsValidFlagValue("bool_retired_flag", "true"));
}

}  // namespace
