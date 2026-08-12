// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/port_range.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting {

PortRange::PortRange() = default;
PortRange::~PortRange() = default;

PortRange::PortRange(const PortRange&) = default;
PortRange& PortRange::operator=(const PortRange&) = default;
PortRange::PortRange(PortRange&&) = default;
PortRange& PortRange::operator=(PortRange&&) = default;

PortRange::PortRange(uint16_t min_port, uint16_t max_port)
    : min_port_(min_port), max_port_(max_port) {}

// static
std::optional<PortRange> PortRange::Create(uint16_t min_port,
                                           uint16_t max_port) {
  bool is_range_valid = (min_port == 0 && max_port == 0) ||
                        (min_port > 0 && min_port <= max_port);
  if (!is_range_valid) {
    return std::nullopt;
  }
  return PortRange(min_port, max_port);
}

// static
std::optional<PortRange> PortRange::Parse(std::string_view port_range) {
  if (port_range.empty()) {
    return PortRange();
  }

  auto pieces = base::SplitStringOnce(port_range, '-');
  if (!pieces) {
    return std::nullopt;
  }

  std::string_view min_port_string =
      base::TrimWhitespaceASCII(pieces->first, base::TRIM_ALL);
  std::string_view max_port_string =
      base::TrimWhitespaceASCII(pieces->second, base::TRIM_ALL);

  unsigned min_port, max_port;
  if (!base::StringToUint(min_port_string, &min_port) ||
      !base::StringToUint(max_port_string, &max_port)) {
    return std::nullopt;
  }

  if (min_port == 0 || min_port > USHRT_MAX || max_port == 0 ||
      max_port > USHRT_MAX) {
    return std::nullopt;
  }

  return Create(static_cast<uint16_t>(min_port),
                static_cast<uint16_t>(max_port));
}

std::ostream& operator<<(std::ostream& os, const PortRange& port_range) {
  if (port_range.is_null()) {
    os << "<no port range specified>";
  } else {
    os << "[" << port_range.min_port() << ", " << port_range.max_port() << "]";
  }
  return os;
}

}  // namespace remoting
