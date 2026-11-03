// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPE_ENVIRONMENT_H_
#define REMOTING_HOST_LINUX_HOST_TYPE_ENVIRONMENT_H_

#include <sys/types.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"

namespace remoting {

// Wraps the system state that host types inspect and modify (systemd units and
// the session routing config file), so that host type logic can be unit
// tested. The unit methods run `systemctl` and block.
class HostTypeEnvironment {
 public:
  // Creates an environment that operates on the real system with `systemctl`.
  static std::unique_ptr<HostTypeEnvironment> Create();

  virtual ~HostTypeEnvironment();

  // Returns whether the systemd unit `unit` (e.g. `foo@user.service`) is
  // active.
  virtual bool IsUnitActive(const std::string& unit) const = 0;

  // Returns whether `systemctl is-enabled` succeeds for `unit`. Note that this
  // is also true for states other than `enabled`, e.g. `static`, `alias` and
  // `enabled-runtime`.
  virtual bool IsUnitEnabled(const std::string& unit) const = 0;

  // Enables and starts `unit`. Returns false on failure.
  virtual bool EnableAndStartUnit(const std::string& unit) = 0;

  // Disables and stops `unit`. Returns false on failure.
  virtual bool DisableAndStopUnit(const std::string& unit) = 0;

  // Returns the path of the session routing config file.
  virtual base::FilePath GetSessionRoutingConfigPath() const = 0;

  // Returns the UID that must own the session routing config file and its
  // directory: 0 in production. Passed to
  // SessionRoutingConfig::LoadAndValidate().
  virtual uid_t GetExpectedOwnerUid() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPE_ENVIRONMENT_H_
