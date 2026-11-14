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

// The path of the installed migrate-host tool, which switches the host type of
// a configured host.
inline constexpr char kMigrateHostPath[] =
    "/opt/google/chrome-remote-desktop/migrate-host";

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
  // in which case the steps that operate on the units of a specific user are
  // skipped.
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

  // Returns the host type that is set up on this computer, whether or not its
  // host is running, or nullptr if none is. Reads the host type record (see
  // WriteHostTypeRecord()), or falls back to GetFallbackConfigured() if no
  // record exists (e.g. hosts set up before the record was introduced, or
  // unprivileged single-process setups). Returns an error if the record is
  // invalid or insecure. Performs blocking operations.
  static base::expected<const HostType*, std::string> GetConfigured(
      const HostTypeEnvironment& env);

  // Infers the configured host type from the host config files and the state
  // of the host units when there is no host type record, e.g. for hosts set up
  // before the record was introduced (see WriteHostTypeRecord()) or
  // unprivileged single-process setups (see RunStartHostHook()). Checks
  // whether any `chrome-remote-desktop@*.service` unit is enabled or active
  // for single-process hosts. Returns nullptr if no host is configured.
  // Performs blocking operations.
  static const HostType* GetFallbackConfigured(const HostTypeEnvironment& env);

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

  // Returns whether the start hook supports the local user and the host
  // config described by `context`, and prints an error if not. Unlike the
  // rest of RunStartHostHook(), which calls this first, this doesn't check or
  // change the system state, so it can also be called before anything is
  // changed, e.g. before the host config is written. The default
  // implementation checks that, if this host type requires a local user (see
  // requires_local_user()), `context.local_user_name` can be used in unit
  // names (see IsUserNameSafeForUnitName()).
  virtual bool ValidateStartHostHookContext(
      const HostTypeContext& context) const;

  // Atomically writes this host type's name to `env.GetHostTypeRecordPath()`
  // (mode 0644) after creating its directory (mode 0755) if absent and
  // verifying it with CheckSecureDirectory(). The record names the configured
  // host type (see GetConfigured()), and is kept when the host is stopped or
  // deleted. Prints an error and returns false on failure. Performs blocking
  // operations.
  bool WriteHostTypeRecord(const HostTypeEnvironment& env) const;

  // Called when setting up a host of this type as root (unprivileged
  // single-process setup starts the unit via the host script instead).
  // Validates `context`, sets up host-type state, starts the host unit, and
  // records this host type. Returns false on failure.
  bool RunStartHostHook(const HostTypeContext& context) const;

  // Stops and disables the host unit of this host type, then cleans up its
  // state. Called as root when migrating away from this host type. The unit is
  // stopped first, so that the host can clean up (e.g. end the sessions it
  // created) before its state is removed. Returns false on failure, in which
  // case the state is kept.
  bool RunStopHostHook(const HostTypeContext& context) const;

 protected:
  // Enables and starts the host unit of this host type, without changing its
  // state. See RunStartHostHook(). Returns false on failure.
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

// Checks whether `local_user_name` is valid for setting up or migrating to
// `target_host_type`. Returns `base::ok()` if so, or a user-facing error
// message.
// - `target_host_type`: The host type being set up or migrated to.
// - `current_host_type`: The host type currently configured on the machine, or
//   nullptr if none is configured.
// - `local_user_name`: The local Linux account to use, or empty if none was
//   specified (e.g. when running `start-host` or `migrate-host` as root
//   without `--user-name`).
base::expected<void, std::string> CheckLocalUserForHostTypeSetup(
    const HostType& target_host_type,
    const HostType* current_host_type,
    std::string_view local_user_name);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPES_H_
