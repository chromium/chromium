// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/input_builder.h"

#include <limits.h>
#include <stdlib.h>
#include <cstdio>

#include "base/check.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"

namespace media {
namespace cast {
namespace test {

static const char kEnablePromptsSwitch[] = "enable-prompts";

InputBuilder::InputBuilder(const std::string& title,
                           const std::string& default_value,
                           int low_range,
                           int high_range)
    : title_(title),
      default_value_(default_value),
      low_range_(low_range),
      high_range_(high_range) {}

InputBuilder::~InputBuilder() = default;

std::string InputBuilder::GetStringInput() const {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(kEnablePromptsSwitch))
    return default_value_;

  printf("\n%s\n", title_.c_str());
  if (!default_value_.empty())
    printf("Hit enter for default (%s):\n", default_value_.c_str());

  printf("# ");
  fflush(stdout);
  char raw_input[128];
  if (!fgets(raw_input, 128, stdin)) {
    NOTREACHED();
    return std::string();
  }

  std::string input = raw_input;
  input = input.substr(0, input.size() - 1);  // Strip last \n.
  if (input.empty() && !default_value_.empty())
    return default_value_;

  if (!ValidateInput(input)) {
    printf("Invalid input. Please try again.\n");
    return GetStringInput();
  }
  return input;
}

int InputBuilder::GetIntInput() const {
  std::string string_input = GetStringInput();
  int int_value;
  CHECK(base::StringToInt(string_input, &int_value));
  return int_value;
}

bool InputBuilder::ValidateInput(const std::string& input) const {
  // Check for a valid range.
  if (low_range_ == INT_MIN && high_range_ == INT_MAX)
    return true;
  int value;
  if (!base::StringToInt(input, &value))
    return false;
  return value >= low_range_ && value <= high_range_;
}

}  // namespace test
}  // namespace cast
}  // namespace media
