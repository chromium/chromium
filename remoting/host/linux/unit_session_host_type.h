// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_UNIT_SESSION_HOST_TYPE_H_
#define REMOTING_HOST_LINUX_UNIT_SESSION_HOST_TYPE_H_

#include <string>
#include <string_view>

#include "remoting/host/linux/host_types.h"

namespace remoting {

// Multi-process host whose session is started and kept running for the local
// user by a systemd unit, e.g. `chrome-remote-desktop-x11-session@<user>`. The
// session routing config file routes all connections to that user.
class UnitSessionHostType : public MultiProcessHostType {
 public:
  // `unit_prefix` is the session unit's name up to and including the '@', e.g.
  // `chrome-remote-desktop-x11-session@` for
  // `chrome-remote-desktop-x11-session@<user>.service`. The strings must
  // outlive this object (they are string literals).
  UnitSessionHostType(std::string_view name,
                      std::string_view description,
                      std::string_view unit_prefix);
  UnitSessionHostType(const UnitSessionHostType&) = delete;
  UnitSessionHostType& operator=(const UnitSessionHostType&) = delete;
  ~UnitSessionHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool requires_local_user() const override;
  bool ValidateStartHostHookContext(
      const HostTypeContext& context) const override;

  // Returns the session unit for `local_user_name` (`*` for a glob pattern
  // that matches the session units of all users).
  std::string GetSessionUnit(std::string_view local_user_name) const;

  // Stops and disables the session units of all unit session host types for all
  // users that are enabled or active, except `except_unit` (may be empty).
  // Returns false on failure, including if the units can't be listed.
  static bool StopAllSessionUnits(HostTypeEnvironment& env,
                                  std::string_view except_unit = {});

 protected:
  // MultiProcessHostType implementation.

  // Like MultiProcessHostType::SetUpState(), but keeps the session unit of
  // the local user, then writes the session routing config file and starts
  // that session unit.
  bool SetUpState(const HostTypeContext& context) const override;

  // Stops and disables the session units of this host type for all users,
  // then deletes the session routing config file.
  bool CleanUpState(const HostTypeContext& context) const override;

 private:
  std::string_view name_;
  std::string_view description_;
  std::string_view unit_prefix_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_UNIT_SESSION_HOST_TYPE_H_
