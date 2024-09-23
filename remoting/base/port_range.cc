// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/port_range.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace remoting {

bool PortRange::operator==(const PortRange&) const = default;

void PortRange::reset() {
  min_port = 0;
  max_port = 0;
}

bool PortRange::Parse(const std::string& port_range, PortRange* result) {
  DCHECK(result);

  if (port_range.empty()) {
    result->min_port = 0;
    result->max_port = 0;
    return true;
  }

  size_t separator_index = port_range.find('-');
  if (separator_index == std::string::npos) {
    return false;
  }

  std::string min_port_string, max_port_string;
  base::TrimWhitespaceASCII(port_range.substr(0, separator_index),
                            base::TRIM_ALL, &min_port_string);
  base::TrimWhitespaceASCII(port_range.substr(separator_index + 1),
                            base::TRIM_ALL, &max_port_string);

  unsigned min_port, max_port;
  if (!base::StringToUint(min_port_string, &min_port) ||
      !base::StringToUint(max_port_string, &max_port)) {
    return false;
  }

  if (min_port == 0 || min_port > max_port || max_port > USHRT_MAX) {
    return false;
  }

  result->min_port = static_cast<uint16_t>(min_port);
  result->max_port = static_cast<uint16_t>(max_port);
  return true;
}

std::ostream& operator<<(std::ostream& os, const PortRange& port_range) {
  if (port_range.is_null()) {
    os << "<no port range specified>";
  } else {
    os << "[" << port_range.min_port << ", " << port_range.max_port << "]";
  }
  return os;
}

}  // namespace remoting
