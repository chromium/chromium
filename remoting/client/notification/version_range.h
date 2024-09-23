// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_VERSION_RANGE_H_
#define REMOTING_CLIENT_NOTIFICATION_VERSION_RANGE_H_

#include <optional>
#include <string>

#include "base/version.h"

namespace remoting {

// Class representing a range of dotted version numbers. Supporting parsing and
// in-range checking.
class VersionRange final {
 public:
  // Parses a range spec into range.
  //
  // Format (regex):
  //   ([\(\[]((\d+\.)*\d+)?-((\d+\.)*\d+)?[\)\]]|(\d+\.)*\d+)
  //
  // Examples:
  //   1.2.3      Exactly 1.2.3
  //   [-1.2.3)   Anything up to but not including 1.2.3
  //   [1.2-3)    Anything between 1.2 (inclusive) and 3 (exclusive)
  //   (1.2-3]    Anything between 1.2 (exclusive) and 3 (inclusive)
  //   [1.2-)     1.2 (inclusive) and higher
  //   [-]        Anything
  //
  // Min version must be less than or equal to max version.
  explicit VersionRange(const std::string& range_spec);

  VersionRange(const VersionRange&) = delete;
  VersionRange& operator=(const VersionRange&) = delete;

  ~VersionRange();

  bool IsValid() const;
  bool ContainsVersion(const std::string& version_string) const;

 private:
  std::optional<base::Version> min_version_;
  std::optional<base::Version> max_version_;

  bool is_min_version_inclusive_ = false;
  bool is_max_version_inclusive_ = false;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_VERSION_RANGE_H_
