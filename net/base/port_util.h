// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PORT_UTIL_H_
#define NET_BASE_PORT_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// Checks if |port| is in the valid range (0 to 65535, though 0 is technically
// reserved).  Should be used before casting a port to a uint16_t.
NET_EXPORT bool IsPortValid(int port);

// Returns true if the port is in the range [0, 1023]. These ports are
// registered by IANA and typically need root access to listen on.
NET_EXPORT bool IsWellKnownPort(int port);

// Checks if the port is allowed for the specified scheme.  Ports set as allowed
// with SetExplicitlyAllowedPorts() or by using ScopedPortException() will be
// considered allowed for any scheme.
NET_EXPORT bool IsPortAllowedForScheme(int port, base::StringPiece url_scheme);

// Returns the number of explicitly allowed ports; for testing.
NET_EXPORT_PRIVATE size_t GetCountOfExplicitlyAllowedPorts();

NET_EXPORT void SetExplicitlyAllowedPorts(const std::string& allowed_ports);

class NET_EXPORT ScopedPortException {
 public:
  explicit ScopedPortException(int port);
  ~ScopedPortException();

 private:
  int port_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPortException);
};

}  // namespace net

#endif  // NET_BASE_PORT_UTIL_H_
