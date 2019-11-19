// Copyright 2018 The Abseil Authors.
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

#include "absl/base/log_severity.h"

#include <sstream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {
using testing::Eq;

std::string StreamHelper(absl::LogSeverity value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

TEST(StreamTest, Works) {
  EXPECT_THAT(StreamHelper(static_cast<absl::LogSeverity>(-100)),
              Eq("absl::LogSeverity(-100)"));
  EXPECT_THAT(StreamHelper(absl::LogSeverity::kInfo), Eq("INFO"));
  EXPECT_THAT(StreamHelper(absl::LogSeverity::kWarning), Eq("WARNING"));
  EXPECT_THAT(StreamHelper(absl::LogSeverity::kError), Eq("ERROR"));
  EXPECT_THAT(StreamHelper(absl::LogSeverity::kFatal), Eq("FATAL"));
  EXPECT_THAT(StreamHelper(static_cast<absl::LogSeverity>(4)),
              Eq("absl::LogSeverity(4)"));
}

}  // namespace
