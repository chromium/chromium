// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_

#include <cstdint>
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

  static std::string GetErrorString(IwaVersionParseError error);

  // Provide access to the underlying base::Version object.
  const base::Version* operator->() const { return &version_; }
  const base::Version& operator*() const { return version_; }

 private:
  explicit IwaVersion(base::Version version);

  base::Version version_;
};

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_IWA_VERSION_H_
