// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_sandbox_hook_linux.h"

#include <dlfcn.h>

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/features.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace network {

sandbox::syscall_broker::BrokerCommandSet GetNetworkBrokerCommandSet() {
  return MakeBrokerCommandSet({
      sandbox::syscall_broker::COMMAND_ACCESS,
      sandbox::syscall_broker::COMMAND_MKDIR,
      sandbox::syscall_broker::COMMAND_OPEN,
      sandbox::syscall_broker::COMMAND_READLINK,
      sandbox::syscall_broker::COMMAND_RENAME,
      sandbox::syscall_broker::COMMAND_RMDIR,
      sandbox::syscall_broker::COMMAND_STAT,
      sandbox::syscall_broker::COMMAND_UNLINK,
      sandbox::syscall_broker::COMMAND_INOTIFY_ADD_WATCH,
  });
}

std::vector<BrokerFilePermission> GetNetworkFilePermissions(
    std::vector<std::string> network_context_parent_dirs) {
  if (!base::FeatureList::IsEnabled(
          sandbox::policy::features::kNetworkServiceFileAllowlist)) {
    return {BrokerFilePermission::AllPermissions("/"),
            BrokerFilePermission::AllPermissionsRecursive("/")};
  }

  const std::array<base::FilePath, 5> system_config_files = {
      base::FilePath("/etc/hosts"), base::FilePath("/etc/resolv.conf"),
      base::FilePath("/etc/nsswitch.conf"), base::FilePath("/etc/host.conf"),
      base::FilePath("/etc/gai.conf")};

  // Each system config file needs read permissions and watch permissions, and
  // read and watch permissions for its symlink target, if any. This is up to
  // 4 permissions.
  constexpr size_t kMaxPermissionsPerSystemConfigFile = 4;

  std::vector<BrokerFilePermission> perms;
  perms.reserve(system_config_files.size() *
                    kMaxPermissionsPerSystemConfigFile +
                network_context_parent_dirs.size());

  for (const base::FilePath& system_config_file : system_config_files) {
    perms.push_back(BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
        system_config_file.value()));
    perms.push_back(BrokerFilePermission::ReadOnly(system_config_file.value()));

    std::optional<base::FilePath> target_path =
        base::ReadSymbolicLinkAbsolute(system_config_file);
    if (!target_path.has_value()) {
      continue;
    }

    VLOG(1) << "Adding permissions for symlink: " << target_path->value();
    perms.push_back(BrokerFilePermission::InotifyAddWatchWithIntermediateDirs(
        target_path->value()));
    perms.push_back(BrokerFilePermission::ReadOnly(target_path->value()));
  }

  for (std::string dir_str : network_context_parent_dirs) {
    if (*dir_str.rbegin() != '/') {
      dir_str += "/";
    }
    VLOG(1) << "Granting ReadWriteCreateRecursive permissions to " << dir_str;
    perms.push_back(
        BrokerFilePermission::ReadWriteCreateRecursive(std::move(dir_str)));
  }

  return perms;
}

#if BUILDFLAG(IS_CHROMEOS)
void LoadNetworkLibraries() {
  const std::string libraries[]{
      // On ChromeOS DNS resolution will occur in process, so load the libraries
      // now. Note that depending on the glibc version, these libraries may have
      // been built directly into libc.so, so it's not an error if they fail to
      // load.
      "libnss_files.so.2", "libnss_dns.so.2"};
  for (const auto& library_name : libraries) {
    if (!dlopen(library_name.c_str(),
                RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE)) {
      VLOG(1) << "LoadNetworkLibraries() dlopen() of " << library_name
              << " failed with error: " << dlerror();
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool NetworkPreSandboxHook(std::vector<std::string> network_context_parent_dirs,
                           sandbox::policy::SandboxLinux::Options options) {
#if BUILDFLAG(IS_CHROMEOS)
  LoadNetworkLibraries();
#endif

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  VLOG(1) << "Using network service sandbox.";

  instance->StartBrokerProcess(
      GetNetworkBrokerCommandSet(),
      GetNetworkFilePermissions(std::move(network_context_parent_dirs)),
      options);

  return true;
}

}  // namespace network
