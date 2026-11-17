// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_AUTO_SESSION_HOST_TYPE_H_
#define REMOTING_HOST_LINUX_AUTO_SESSION_HOST_TYPE_H_

#include <string_view>

#include "remoting/host/linux/host_types.h"

namespace remoting {

// Multi-process host that routes connections to the local user, and lets the
// daemon ask the display manager to create a session for the user on connect
// if they have none (`createRemoteUserSessions` in the session routing config
// file).
class AutoSessionHostType : public MultiProcessHostType {
 public:
  ~AutoSessionHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool requires_local_user() const override;
  bool ValidateStartHostHookContext(
      const HostTypeContext& context) const override;

 protected:
  // MultiProcessHostType implementation.
  bool SetUpState(const HostTypeContext& context) const override;
};

// Returns the auto-session host type.
const HostType& GetAutoSessionHostType();

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_AUTO_SESSION_HOST_TYPE_H_
