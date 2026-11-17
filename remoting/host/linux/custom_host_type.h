// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CUSTOM_HOST_TYPE_H_
#define REMOTING_HOST_LINUX_CUSTOM_HOST_TYPE_H_

#include <string_view>

#include "remoting/host/linux/host_types.h"

namespace remoting {

// Multi-process host that connects to a graphical session that the user starts
// themselves, or to a local graphical session if curtain mode is disabled.
// Routes connections to the local user via the session routing config file
// (without creating remote sessions).
class CustomHostType : public MultiProcessHostType {
 public:
  ~CustomHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool requires_local_user() const override;
  bool ValidateStartHostHookContext(
      const HostTypeContext& context) const override;

 protected:
  // MultiProcessHostType implementation.
  bool SetUpState(const HostTypeContext& context) const override;
};

// Returns the custom host type.
const HostType& GetCustomHostType();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CUSTOM_HOST_TYPE_H_
