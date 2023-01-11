// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PORT_RANGE_H_
#define REMOTING_PROTOCOL_PORT_RANGE_H_

#include <stdint.h>

#include <ostream>
#include <string>

namespace remoting {

// Wrapper for a value of UdpPortRange policy.
struct PortRange {
  // Both |min_port| and |max_port| are inclusive.
  uint16_t min_port;
  uint16_t max_port;

  // Returns true if |port_range| passed to Parse was an empty string
  // (or if |this| has been initialized by the default constructor below).
  inline bool is_null() const { return (min_port == 0) && (max_port == 0); }

  // Parse string in the form "<min_port>-<max_port>". E.g. "12400-12409".
  // Returns true if string was parsed successfully.
  //
  // Returns false and doesn't modify |result| if parsing fails (i.e. when
  // |port_range| doesn't represent a valid port range).
  static bool Parse(const std::string& port_range, PortRange* result);

  PortRange() : min_port(0), max_port(0) {}
};

std::ostream& operator<<(std::ostream& os, const PortRange& port_range);

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PORT_RANGE_H_
