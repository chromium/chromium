//
// Copyright 2019 The Abseil Authors.
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

#include "absl/base/internal/raw_logging.h"
#include "absl/flags/config.h"
#include "absl/flags/usage_config.h"
#include "absl/strings/str_cat.h"

namespace absl {
namespace flags_internal {

bool GetCommandLineOption(absl::string_view name, std::string* value) {
  if (name.empty()) return false;
  assert(value);

  CommandLineFlag* flag = flags_internal::FindCommandLineFlag(name);
  if (flag == nullptr || flag->IsRetired()) {
    return false;
  }

  *value = flag->CurrentValue();
  return true;
}

bool GetCommandLineFlagInfo(absl::string_view name,
                            CommandLineFlagInfo* OUTPUT) {
  if (name.empty()) return false;

  CommandLineFlag* flag = flags_internal::FindCommandLineFlag(name);
  if (flag == nullptr || flag->IsRetired()) {
    return false;
  }

  assert(OUTPUT);
  FillCommandLineFlagInfo(flag, OUTPUT);
  return true;
}

CommandLineFlagInfo GetCommandLineFlagInfoOrDie(absl::string_view name) {
  CommandLineFlagInfo info;
  if (!GetCommandLineFlagInfo(name, &info)) {
    ABSL_INTERNAL_LOG(FATAL, absl::StrCat("Flag '", name, "' does not exist"));
  }
  return info;
}

// --------------------------------------------------------------------

bool SetCommandLineOption(absl::string_view name, absl::string_view value) {
  return SetCommandLineOptionWithMode(name, value,
                                      flags_internal::SET_FLAGS_VALUE);
}

bool SetCommandLineOptionWithMode(absl::string_view name,
                                  absl::string_view value,
                                  FlagSettingMode set_mode) {
  CommandLineFlag* flag = flags_internal::FindCommandLineFlag(name);

  if (!flag || flag->IsRetired()) return false;

  std::string error;
  if (!flag->SetFromString(value, set_mode, kProgrammaticChange, &error)) {
    // Errors here are all of the form: the provided name was a recognized
    // flag, but the value was invalid (bad type, or validation failed).
    flags_internal::ReportUsageError(error, false);
    return false;
  }

  return true;
}

// --------------------------------------------------------------------

bool IsValidFlagValue(absl::string_view name, absl::string_view value) {
  CommandLineFlag* flag = flags_internal::FindCommandLineFlag(name);

  return flag != nullptr &&
         (flag->IsRetired() || flag->ValidateInputValue(value));
}

// --------------------------------------------------------------------

bool SpecifiedOnCommandLine(absl::string_view name) {
  CommandLineFlag* flag = flags_internal::FindCommandLineFlag(name);
  if (flag != nullptr && !flag->IsRetired()) {
    return flag->IsSpecifiedOnCommandLine();
  }
  return false;
}

}  // namespace flags_internal
}  // namespace absl
