// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPES_H_
#define REMOTING_HOST_LINUX_HOST_TYPES_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"

namespace remoting {

class HostTypeEnvironment;

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
  // type doesn't require a local user (see HostType::requires_local_user()).
  std::string local_user_name;

  // Whether the host config is for a corp host (see IsCorpHostConfig()).
  bool is_corp_host = false;
};

// Interface representing a host type on Linux.
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
  // `local_user_name` is the local account whose per-user units are checked,
  // e.g. `chrome-remote-desktop@<local_user_name>.service` for the
  // single-process host. If it is empty, per-user units are not checked.
  // Detection takes the user rather than a HostTypeContext because no context
  // exists yet: the context's `is_corp_host` comes from the host config, and
  // migrate-host reads the config from the location used by the detected host
  // type.
  static const HostType* DetectCurrent(const HostTypeEnvironment& env,
                                       std::string_view local_user_name);

  virtual ~HostType() = default;

  // The name of the host type.
  virtual std::string_view name() const = 0;

  // Human-readable description of the host type.
  virtual std::string_view description() const = 0;

  // Returns true if the host type has a multi-process architecture.
  virtual bool is_multi_process() const = 0;

  // Returns true if start-host and migrate-host must know the local user to
  // set up this host type.
  virtual bool requires_local_user() const = 0;

  // Returns whether this host type is the one that is currently running,
  // assuming that no host type earlier in the detection order matched (see
  // DetectCurrent(), which also describes `local_user_name`). Performs
  // blocking operations.
  virtual bool IsCurrent(const HostTypeEnvironment& env,
                         std::string_view local_user_name) const = 0;

  // Runs the start hook of this host type. Called as root before the host
  // service is started. Returns false on failure, in which case the host
  // service should not be started. The default implementation does nothing
  // and returns true.
  virtual bool RunStartHostHook(const HostTypeContext& context) const;

  // Runs the stop hook of this host type. Called as root after the host
  // service has been stopped, when moving away from this host type. Returns
  // false on failure. The default implementation does nothing and returns
  // true.
  virtual bool RunStopHostHook(const HostTypeContext& context) const;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPES_H_
