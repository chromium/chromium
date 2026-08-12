// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PORT_RANGE_H_
#define REMOTING_BASE_PORT_RANGE_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string_view>

namespace remoting {

// LINT.IfChange(PortRange)
// Port range policy to be applied to the CRD host.
class PortRange {
 public:
  // Creates a range with no restrictions (all ports allowed) by default.
  PortRange();
  ~PortRange();

  PortRange(const PortRange&);
  PortRange& operator=(const PortRange&);
  PortRange(PortRange&&);
  PortRange& operator=(PortRange&&);

  // Creates a PortRange. Returns std::nullopt if the range is invalid.
  // A valid PortRange is either null (`min_port == 0 && max_port == 0`, meaning
  // all ports allowed) or satisfies `0 < min_port <= max_port`.
  static std::optional<PortRange> Create(uint16_t min_port, uint16_t max_port);

  // Parses a string in the form "<min_port>-<max_port>". E.g. "12400-12409".
  // Returns a valid PortRange on success, or std::nullopt if parsing fails or
  // the port range is invalid. An empty string returns a null PortRange. Note
  // that unlike Create(), `0-0` is invalid.
  static std::optional<PortRange> Parse(std::string_view port_range);

  bool operator==(const PortRange&) const = default;

  // Returns true if the port range is null (i.e. `min_port == 0 && max_port ==
  // 0`, all ports allowed).
  bool is_null() const { return min_port_ == 0 && max_port_ == 0; }

  // Minimum port number (inclusive). 0 if is_null().
  uint16_t min_port() const { return min_port_; }

  // Maximum port number (inclusive). 0 if is_null().
  uint16_t max_port() const { return max_port_; }

 private:
  PortRange(uint16_t min_port, uint16_t max_port);

  uint16_t min_port_ = 0;
  uint16_t max_port_ = 0;
};
// LINT.ThenChange(//remoting/host/mojom/common.mojom:PortRange)

std::ostream& operator<<(std::ostream& os, const PortRange& port_range);

}  // namespace remoting

#endif  // REMOTING_BASE_PORT_RANGE_H_
