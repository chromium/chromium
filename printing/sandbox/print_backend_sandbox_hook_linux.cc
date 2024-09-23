// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/sandbox/print_backend_sandbox_hook_linux.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/linux/sandbox_linux.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_connection_pool.h"
#endif

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

  return broker_command_set;
}

std::vector<BrokerFilePermission> GetPrintBackendFilePermissions() {
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  // No extra permissions required, as the needed socket connections to the CUPS
  // server are established before entering the sandbox.
  return std::vector<BrokerFilePermission>();
#else
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

  return permissions;
#endif  // BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
}

}  // namespace

bool PrintBackendPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  // Create the socket connections to the CUPS server before engaging the
  // sandbox, since new connections cannot be made after that.
  CupsConnectionPool::Create();
#endif

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();

  instance->StartBrokerProcess(GetPrintBackendBrokerCommandSet(),
                               GetPrintBackendFilePermissions(), options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace printing
