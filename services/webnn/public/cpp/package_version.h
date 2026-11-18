// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_PACKAGE_VERSION_H_
#define SERVICES_WEBNN_PUBLIC_CPP_PACKAGE_VERSION_H_

#include <stdint.h>

#include <compare>

namespace webnn {

// A Windows package version. Members are declared in comparison order so the
// defaulted comparisons order versions correctly.
struct PackageVersion {
  uint16_t major = 0;
  uint16_t minor = 0;
  uint16_t build = 0;
  uint16_t revision = 0;

  auto operator<=>(const PackageVersion&) const = default;
  bool operator==(const PackageVersion&) const = default;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_PACKAGE_VERSION_H_
