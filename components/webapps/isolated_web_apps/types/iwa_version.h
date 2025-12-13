// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"

namespace web_app {

class IwaVersion {
 public:
  // Enum for parsing errors
  enum class IwaVersionParseError {
    kNoComponents,
    kEmptyComponent,
    kLeadingZero,
    kNonDigit,
    kCannotConvertToNumber,
    kTooManyComponents,
  };

  // Maximum number of version components allowed
  static constexpr size_t kMaxNumberOfComponents = 4;

  static base::expected<IwaVersion, IwaVersionParseError> Create(
      std::string_view version_string);

  static base::expected<IwaVersion, IwaVersionParseError> Create(
      std::vector<uint32_t> components);

  static std::string GetErrorString(IwaVersionParseError error);

  std::string GetString() const { return version_.GetString(); }

  // Returns the underlying Version.
  const base::Version& version() const { return version_; }

  friend bool operator==(const IwaVersion& lhs,
                         const IwaVersion& rhs) = default;
  friend auto operator<=>(const IwaVersion& lhs, const IwaVersion& rhs) {
    return lhs.version_.components() <=> rhs.version_.components();
  }

 private:
  explicit IwaVersion(base::Version version);

  base::Version version_;
};

std::ostream& operator<<(std::ostream& stream, const IwaVersion& v);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_
