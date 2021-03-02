// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/sandbox/print_backend_sandbox_hook_linux.h"
#include "sandbox/policy/export.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "services/network/network_sandbox_hook_linux.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace printing {

namespace {

sandbox::syscall_broker::BrokerCommandSet GetPrintBackendBrokerCommandSet() {
  // Need read access to look at system PPD files.
  // Need ability to create/write/delete for temporary files in order to
  // support PPD handling in `printing::ParsePpdCapabilities()`.
  sandbox::syscall_broker::BrokerCommandSet broker_command_set =
      MakeBrokerCommandSet({
          sandbox::syscall_broker::COMMAND_ACCESS,
          sandbox::syscall_broker::COMMAND_OPEN,
          sandbox::syscall_broker::COMMAND_READLINK,
          sandbox::syscall_broker::COMMAND_STAT,
          sandbox::syscall_broker::COMMAND_UNLINK,
      });

  // Need networking for a TCP connection to CUPS servers.
  broker_command_set |= network::GetNetworkBrokerCommandSet();

  return broker_command_set;
}

std::vector<BrokerFilePermission> GetPrintBackendFilePermissions() {
  base::FilePath temp_dir_path;
  CHECK(base::GetTempDir(&temp_dir_path));
  base::FilePath home_dir_path;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir_path));
  base::FilePath cups_options_path = home_dir_path.Append(".cups/lpoptions");

  std::vector<BrokerFilePermission> permissions{
      // To support reading system PPDs.  This list is per the CUPS docs with
      // macOS-specific paths omitted.
      // https://www.cups.org/doc/man-cupsd-helper.html
      BrokerFilePermission::ReadOnlyRecursive("/opt/share/ppd/"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/local/share/ppd/"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/cups/drv/"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/cups/model/"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/ppd/"),
      // To support reading user's default printer.
      // https://www.cups.org/doc/cupspm.html#cupsEnumDests
      // https://www.cups.org/doc/options.html
      BrokerFilePermission::ReadOnly(cups_options_path.value()),
      // To support PPD handling in `printing::ParsePpdCapabilities()`.
      BrokerFilePermission::ReadWriteCreateTemporary(temp_dir_path.value()),
  };

  // To support networking for a TCP connection to CUPS servers.
  auto network_permissions = network::GetNetworkFilePermissions();
  permissions.insert(permissions.end(), network_permissions.begin(),
                     network_permissions.end());

  return permissions;
}

}  // namespace

bool PrintBackendPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  instance->StartBrokerProcess(
      GetPrintBackendBrokerCommandSet(), GetPrintBackendFilePermissions(),
      sandbox::policy::SandboxLinux::PreSandboxHook(), options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace printing
