// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPES_H_
#define REMOTING_HOST_LINUX_HOST_TYPES_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"

namespace remoting {

class HostTypeEnvironment;

// The systemd unit of the multi-process host.
inline constexpr char kMultiProcessHostUnit[] = "chrome-remote-desktop.service";

// Context passed to the HostType hooks.
struct HostTypeContext {
  HostTypeContext(HostTypeEnvironment& env,
                  std::string_view local_user_name,
                  bool is_corp_host);
  HostTypeContext(const HostTypeContext&) = delete;
  HostTypeContext& operator=(const HostTypeContext&) = delete;
  ~HostTypeContext();

  raw_ref<HostTypeEnvironment> env;

  // The local account that the host is set up for. May be empty if the host
  // type doesn't require a local user (see HostType::requires_local_user()),
  // in which case the steps that operate on per-user units are skipped.
  std::string local_user_name;

  // Whether the host config is for a corp host (see IsCorpHostConfig()). Not
  // used by StartHostUnit() and StopHostUnit().
  bool is_corp_host = false;
};

// Interface representing a host type on Linux.
//
// Each host type owns its host unit (`chrome-remote-desktop@<user>.service`
// for the single-process host type, `chrome-remote-desktop.service` for the
// multi-process ones) and its state, e.g. the session routing config file.
// The hooks run as root and perform blocking operations.
class HostType {
 public:
  // Returns the map of all supported host types.
  static const base::flat_map<std::string_view, const HostType*>&
  GetHostTypes();

  // Returns the default host type.
  static const HostType& GetDefaultHostType();

  // Finds a host type by its name. Returns nullptr if not found.
  static const HostType* Find(std::string_view name);

  // Prints the help message for all supported host types.
  static void PrintHostTypeHelp();

  // Detects the type of the host that is currently running. Returns the first
  // host type, in a fixed detection order, whose IsCurrent() returns true, or
  // nullptr if no host is running. Performs blocking operations.
  //
  // `chrome-remote-desktop@<local_user_name>.service` being active means
  // single-process. Otherwise, `chrome-remote-desktop.service` being active
  // means multi-process, and the multi-process host type is the first one
  // whose state (e.g. the session routing config file) matches.
  //
  // `local_user_name` is the local account whose per-user units are checked.
  // If it is empty, per-user units are not checked; otherwise it must pass
  // IsUserNameSafeForUnitName(). Detection takes the user rather than a
  // HostTypeContext because no context exists yet: the context's
  // `is_corp_host` comes from the host config, and migrate-host reads the
  // config from the location used by the detected host type.
  static const HostType* DetectCurrent(const HostTypeEnvironment& env,
                                       std::string_view local_user_name);

  virtual ~HostType() = default;

  // The name of the host type.
  virtual std::string_view name() const = 0;

  // Human-readable description of the host type.
  virtual std::string_view description() const = 0;

  // Returns true if the host type has a multi-process architecture.
  virtual bool is_multi_process() const = 0;

  // Returns true if start-host and migrate-host need a local user to start
  // this host type (see CheckLocalUserForHostTypeSetup()).
  virtual bool requires_local_user() const = 0;

  // Returns whether this host type is the one that is currently running,
  // assuming that no host type earlier in the detection order matched (see
  // DetectCurrent(), which also describes `local_user_name`). Performs
  // blocking operations.
  virtual bool IsCurrent(const HostTypeEnvironment& env,
                         std::string_view local_user_name) const = 0;

  // Returns whether a per-user unit of this host type (e.g.
  // `chrome-remote-desktop@<user>.service`) is enabled or active for any user,
  // or if this can't be determined (see CheckLocalUserForHostTypeSetup()).
  // Performs blocking operations. The default implementation returns false,
  // for host types without per-user units.
  virtual bool HasPerUserUnits(const HostTypeEnvironment& env) const;

  // Starts the host of this host type. Called as root after the host config
  // has been written. If `keep_existing_setup` is true, this host type is
  // already set up (e.g. it is running, and the host is set up again with the
  // same host type), so its state is kept, and only its host unit is started.
  // Otherwise, its start hook is run (see RunStartHostHook()), which also stops
  // leftover units of other host types. No other host type's stop hook is
  // run, since switching the host type of a running host is left to
  // migrate-host. Returns false on failure. Performs blocking operations.
  bool SetUpAndStartHost(const HostTypeContext& context,
                         bool keep_existing_setup) const;

  // Sets up the state of this host type, then enables and starts its host
  // unit. Called as root when setting up this host type, after the host
  // config has been written. Returns false on failure.
  bool RunStartHostHook(const HostTypeContext& context) const;

  // Stops and disables the host unit of this host type, then cleans up its
  // state. Called as root when migrating away from this host type. The unit is
  // stopped first, so that the host can clean up (e.g. end the sessions it
  // created) before its state is removed. Returns false on failure, in which
  // case the state is kept.
  bool RunStopHostHook(const HostTypeContext& context) const;

 protected:
  // Enables and starts the host unit of this host type, without changing its
  // state. See RunStartHostHook(). Also used directly by SetUpAndStartHost()
  // when the existing setup is kept. Returns false on failure.
  virtual bool StartHostUnit(const HostTypeContext& context) const = 0;

  // Stops and disables the host unit of this host type, without changing its
  // state. See RunStopHostHook(). Returns false on failure.
  virtual bool StopHostUnit(const HostTypeContext& context) const = 0;

  // Sets up the state of this host type, see RunStartHostHook(). The default
  // implementation does nothing and returns true.
  virtual bool SetUpState(const HostTypeContext& context) const;

  // Cleans up the state of this host type, see RunStopHostHook(). The default
  // implementation does nothing and returns true.
  virtual bool CleanUpState(const HostTypeContext& context) const;
};

// Returns whether `user_name` can be used in the name of a per-user systemd
// unit (e.g. `chrome-remote-desktop@<user>.service`) that is passed to
// systemctl. Unlike SessionRoutingConfig::IsValidUsername(), this accepts names
// such as "user@domain" from SSSD. It only accepts non-empty names of at most
// 200 characters that consist of ASCII letters, digits and "._@:-" (the
// characters systemd allows in unit names, except its '\' escapes), and,
// conservatively, don't start with '-'.
bool IsUserNameSafeForUnitName(std::string_view user_name);

// Checks the local user that start-host and migrate-host use to set up
// `target_host_type` (`local_user_name`, may be empty). Returns an error
// message if
// - `local_user_name` is given but fails IsUserNameSafeForUnitName(), or
// - no user is given, and `target_host_type` requires one, or a per-user unit
//   of any host type is enabled or active (see HostType::HasPerUserUnits()),
//   since the current host or its leftovers can't be found or cleaned up
//   without the user.
// Without a user, the steps that operate on per-user units are skipped.
// Performs blocking operations.
base::expected<void, std::string> CheckLocalUserForHostTypeSetup(
    const HostTypeEnvironment& env,
    const HostType& target_host_type,
    std::string_view local_user_name);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPES_H_
