// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/version_range.h"

#include <stdint.h>

#include <limits>

#include "base/logging.h"

namespace remoting {

namespace {

constexpr uint16_t kUnboundMinVersionNumber = 0;
constexpr uint16_t kUnboundMaxVersionNumber =
    std::numeric_limits<uint16_t>::max();

}  // namespace

VersionRange::VersionRange(const std::string& range_spec) {
  if (range_spec.empty()) {
    // Invalid.
    return;
  }

  size_t dash_pos = range_spec.find('-');
  if (dash_pos == std::string::npos) {
    // May be a single version string.
    min_version_ = base::make_optional<base::Version>(range_spec);
    max_version_ = base::make_optional<base::Version>(*min_version_);
    is_min_version_inclusive_ = true;
    is_max_version_inclusive_ = true;
    return;
  }

  char left_bracket = range_spec.front();
  if (left_bracket != '[' && left_bracket != '(') {
    // Invalid.
    return;
  }
  is_min_version_inclusive_ = (left_bracket == '[');

  char right_bracket = range_spec.back();
  if (right_bracket != ']' && right_bracket != ')') {
    // Invalid.
    return;
  }
  is_max_version_inclusive_ = (right_bracket == ']');

  DCHECK_GE(range_spec.size(), 3u);
  DCHECK_GT(dash_pos, 0u);
  DCHECK_LT(dash_pos, range_spec.size() - 1);

  std::string min_version_string = range_spec.substr(1, dash_pos - 1);
  if (min_version_string.empty()) {
    // Unbound min version.
    std::vector<uint32_t> version_components{kUnboundMinVersionNumber};
    min_version_ =
        base::make_optional<base::Version>(std::move(version_components));
  } else {
    min_version_ = base::make_optional<base::Version>(min_version_string);
  }

  std::string max_version_string = range_spec.substr(dash_pos + 1);
  max_version_string.pop_back();
  if (max_version_string.empty()) {
    // Unbound max version.
    std::vector<uint32_t> version_components{kUnboundMaxVersionNumber};
    max_version_ =
        base::make_optional<base::Version>(std::move(version_components));
  } else {
    max_version_ = base::make_optional<base::Version>(max_version_string);
  }
}

VersionRange::~VersionRange() = default;

bool VersionRange::IsValid() const {
  return min_version_ && min_version_->IsValid() && max_version_ &&
         max_version_->IsValid() && *min_version_ <= *max_version_;
}

bool VersionRange::ContainsVersion(const std::string& version_string) const {
  if (!IsValid()) {
    return false;
  }
  base::Version version(version_string);
  if (!version.IsValid()) {
    LOG(ERROR) << "Invalid version number: " << version_string;
    return false;
  }
  int min_version_compare_result = min_version_->CompareTo(version);
  if (min_version_compare_result > 0 ||
      (min_version_compare_result == 0 && !is_min_version_inclusive_)) {
    return false;
  }
  int max_version_compare_result = max_version_->CompareTo(version);
  if (max_version_compare_result < 0 ||
      (max_version_compare_result == 0 && !is_max_version_inclusive_)) {
    return false;
  }
  return true;
}

}  // namespace remoting
