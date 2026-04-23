// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPES_H_
#define REMOTING_HOST_LINUX_HOST_TYPES_H_

#include <string_view>

#include "base/containers/flat_map.h"

namespace remoting {

// Interface representing a host type on Linux.
class HostType {
 public:
  // Returns the map of all supported host types.
  static const base::flat_map<std::string_view, const HostType*>&
  GetHostTypes();

  // Prints the help message for all supported host types.
  static void PrintHostTypeHelp();

  virtual ~HostType() = default;

  // Human-readable description of the host type.
  virtual std::string_view description() const = 0;

  // Returns true if the host type has a multi-process architecture.
  virtual bool is_multi_process() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPES_H_
