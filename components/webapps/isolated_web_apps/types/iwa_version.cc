// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/types/iwa_version.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/version.h"

namespace web_app {

namespace {

base::expected<base::Version, IwaVersion::IwaVersionParseError> ParseIwaVersion(
    std::string_view version_string) {
  std::vector<uint32_t> components;

  std::vector<std::string_view> component_strings = base::SplitStringPiece(
      version_string, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (component_strings.empty()) {
    return base::unexpected(IwaVersion::IwaVersionParseError::kNoComponents);
  }

  if (component_strings.size() > IwaVersion::kMaxNumberOfComponents) {
    return base::unexpected(
        IwaVersion::IwaVersionParseError::kTooManyComponents);
  }

  for (const auto& component_string : component_strings) {
    if (component_string.empty()) {
      return base::unexpected(
          IwaVersion::IwaVersionParseError::kEmptyComponent);
    }

    if (component_string.size() > 1 && component_string[0] == '0') {
      return base::unexpected(IwaVersion::IwaVersionParseError::kLeadingZero);
    }

    bool all_digits = true;
    for (char c : component_string) {
      if (!base::IsAsciiDigit(c)) {
        all_digits = false;
        break;
      }
    }
    if (!all_digits) {
      return base::unexpected(IwaVersion::IwaVersionParseError::kNonDigit);
    }

    unsigned int number;
    if (!base::StringToUint(component_string, &number)) {
      return base::unexpected(
          IwaVersion::IwaVersionParseError::kCannotConvertToNumber);
    }
    components.push_back(static_cast<uint32_t>(number));
  }

  return base::Version(std::move(components));
}

base::expected<base::Version, IwaVersion::IwaVersionParseError> ParseIwaVersion(
    std::vector<uint32_t> components) {
  if (components.empty()) {
    return base::unexpected(IwaVersion::IwaVersionParseError::kNoComponents);
  }

  if (components.size() > IwaVersion::kMaxNumberOfComponents) {
    return base::unexpected(
        IwaVersion::IwaVersionParseError::kTooManyComponents);
  }

  return base::Version(std::move(components));
}

}  // namespace

base::expected<IwaVersion, IwaVersion::IwaVersionParseError> IwaVersion::Create(
    std::string_view version_string) {
  return ParseIwaVersion(version_string).transform([](base::Version version) {
    return IwaVersion(std::move(version));
  });
}

base::expected<IwaVersion, IwaVersion::IwaVersionParseError> IwaVersion::Create(
    std::vector<uint32_t> components) {
  return ParseIwaVersion(components).transform([](base::Version version) {
    return IwaVersion(std::move(version));
  });
}

IwaVersion::IwaVersion(base::Version version) : version_(std::move(version)) {}

std::string IwaVersion::GetErrorString(IwaVersionParseError error) {
  switch (error) {
    case IwaVersionParseError::kNoComponents:
      return "A version must consist of at least one number";
    case IwaVersionParseError::kEmptyComponent:
      return "A version component may not be empty";
    case IwaVersionParseError::kNonDigit:
      return "A version component may only contain digits";
    case IwaVersionParseError::kLeadingZero:
      return "A version component may not have leading zeros";
    case IwaVersionParseError::kCannotConvertToNumber:
      return "A version component could not be converted into a number";
    case IwaVersionParseError::kTooManyComponents:
      return base::StringPrintf(
          "A version may not contain more than %d components",
          kMaxNumberOfComponents);
  }
  return "Unknown IWA version parse error";
}

std::ostream& operator<<(std::ostream& stream, const IwaVersion& v) {
  return stream << v.GetString();
}

}  // namespace web_app
