// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/migrate_host_main.h"

#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "remoting/base/branding.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/host_config.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/pairing_registry_delegate_linux.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

namespace {

// Internal switches that are not meant to be used by users directly.
constexpr char kUserNameSwitch[] = "user-name";
constexpr char kSaveMultiProcessPairingsSwitch[] =
    "save-multi-process-pairings";

// Runs the current program with sudo.
// `args`: The arguments to pass to the program.
// `user_name`: If not empty, the program will be run as this user (sudo -u).
// `input`: If not empty, this string will be written to the child's stdin.
bool RunWithSudo(base::span<const std::string_view> args,
                 std::string_view user_name = {},
                 std::string_view input = {}) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<std::string> full_args{"/usr/bin/sudo"};
  if (!user_name.empty()) {
    full_args.push_back("-u");
    full_args.emplace_back(user_name);
  }
  full_args.push_back("--");
  full_args.push_back(
      base::CommandLine::ForCurrentProcess()->GetProgram().value());
  full_args.insert(full_args.end(), args.begin(), args.end());
  command_line.InitFromArgv(full_args);

  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
  base::LaunchOptions options;
  options.allow_new_privs = true;
  if (!input.empty()) {
    if (!base::CreatePipe(&read_fd, &write_fd)) {
      PLOG(ERROR) << "Failed to create pipe";
      return false;
    }
    options.fds_to_remap.emplace_back(read_fd.get(), STDIN_FILENO);
  }

  base::Process process = base::LaunchProcess(command_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch process: "
               << command_line.GetCommandLineString();
    return false;
  }

  if (!input.empty()) {
    // Close the read end in the parent process.
    read_fd.reset();

    if (!base::WriteFileDescriptor(write_fd.get(), input)) {
      PLOG(ERROR) << "Failed to write to pipe: "
                  << command_line.GetCommandLineString();
      return false;
    }

    // Close the write end to signal EOF.
    write_fd.reset();
  }

  int exit_code = -1;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for process exit: "
               << command_line.GetCommandLineString();
    return false;
  }

  if (exit_code == -1) {
    LOG(ERROR) << "Process terminated unexpectedly: "
               << command_line.GetCommandLineString();
    return false;
  }

  if (exit_code != 0) {
    LOG(ERROR) << "Process exited with status " << exit_code << ": "
               << command_line.GetCommandLineString();
    return false;
  }

  return true;
}

bool DisableService(const std::string& unit_name) {
  std::cout << "Disabling service: " << unit_name << "\n";
  base::LaunchOptions options;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.InitFromArgv({"systemctl", "disable", "--now", unit_name});
  if (getuid() != 0) {
    options.allow_new_privs = true;
    command_line.PrependWrapper("/usr/bin/sudo");
  }

  int exit_code = -1;
  auto process = base::LaunchProcess(command_line, options);
  if (!process.IsValid() || !process.WaitForExit(&exit_code)) {
    std::cerr << "Failed to disable host service (" << unit_name << ").\n";
    return false;
  }

  if (exit_code == -1) {
    std::cerr << "Service disable process terminated unexpectedly: "
              << command_line.GetCommandLineString() << "\n";
    return false;
  }

  if (exit_code != 0) {
    std::cerr << "Service disable process exited with status " << exit_code
              << ": " << command_line.GetCommandLineString() << "\n";
    return false;
  }

  return true;
}

void OnDaemonControllerDone(DaemonController::AsyncResult* out_result,
                            base::OnceClosure quit_closure,
                            DaemonController::AsyncResult result) {
  *out_result = result;
  std::move(quit_closure).Run();
}

bool SaveMultiProcessPairingsAsNetworkUser() {
  CHECK_EQ(GetUsername(), GetNetworkProcessUsername());

  std::string json;
  if (!base::ReadStreamToString(stdin, &json)) {
    std::cerr << "Failed to read pairings from stdin.\n";
    return false;
  }

  std::optional<base::Value> pairings_value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!pairings_value || !pairings_value->is_list()) {
    std::cerr << "Failed to parse pairings JSON from stdin.\n";
    return false;
  }

  // Save pairings to multi-process registry.
  PairingRegistryDelegateLinux multi_pairing_delegate;
  for (const auto& pairing_value : pairings_value->GetList()) {
    if (!pairing_value.is_dict()) {
      continue;
    }
    if (!multi_pairing_delegate.Save(
            protocol::PairingRegistry::Pairing::CreateFromValue(
                pairing_value.GetDict()))) {
      std::cerr << "Failed to save a pairing to multi-process registry.\n";
    }
  }

  return true;
}

bool MigrateToMultiProcess(const base::CommandLine& command_line) {
  if (getuid() != 0) {
    std::string user_name_switch =
        base::StringPrintf("--%s=%s", kUserNameSwitch, GetUsername().c_str());

    const base::CommandLine::StringVector& args = command_line.GetArgs();
    CHECK_GE(args.size(), 1u);
    std::string_view host_type = args[0];

    // The code below the if clause will be executed in the child process
    // elevated with sudo.
    if (!RunWithSudo({host_type, user_name_switch})) {
      return false;
    }

    // Deleting the old pairing directory as the user.
    std::cout << "Deleting old single-process config.\n";
    base::FilePath user_config_dir = GetConfigDir();
    base::FilePath config_file =
        user_config_dir.Append(GetHostHash() + ".json");
    base::FilePath user_pairing_dir = user_config_dir.Append(
        PairingRegistryDelegateLinux::kRegistryDirectory);

    if (base::PathExists(user_pairing_dir) &&
        !base::DeletePathRecursively(user_pairing_dir)) {
      std::cerr << "Failed to delete old pairing directory.\n";
      return false;
    }
    // Ideally we should delete the old host config file, but an internal
    // service will re-provision the host if it detects that the config file no
    // longer exists. For now we just clear its content to prevent the
    // single-process host from accidentally running.
    // TODO: b/495898776 - just delete the file once the tooling is fixed.
    if (base::PathExists(config_file) && !base::WriteFile(config_file, "")) {
      std::cerr << "Failed to clear old host config file.\n";
      return false;
    }

    std::cout << "Successfully migrated to multi-process host.\n";
    return true;
  }

  // Code below is run as root.
  std::cout << "Migrating from single-process host to multi-process host.\n";

  std::string user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
  if (user_name.empty()) {
    std::cerr << "--" << kUserNameSwitch
              << " must be provided when run as root.\n";
    return false;
  }

  auto user_info = GetPasswdUserInfo(user_name);
  if (!user_info.has_value()) {
    std::cerr << "Failed to look up user: " << user_name << ": "
              << user_info.error() << "\n";
    return false;
  }

  base::FilePath user_config_dir =
      user_info->home_dir.Append(GetPerUserConfigRelativeDir());
  base::FilePath config_file = user_config_dir.Append(GetHostHash() + ".json");

  // The privileged config files are only readable by root.
  std::optional<base::DictValue> config = HostConfigFromJsonFile(config_file);
  if (!config) {
    std::cerr << "Failed to read single-process host config at "
              << config_file.value() << "\n";
    return false;
  }

  base::FilePath user_pairing_dir =
      user_config_dir.Append(PairingRegistryDelegateLinux::kRegistryDirectory);
  PairingRegistryDelegateLinux user_pairing_delegate(
      user_pairing_dir, /*use_unprivileged_file=*/false);
  base::ListValue pairings = user_pairing_delegate.LoadAll();

  // Code below ensures the multi-process pairing registry directory exists
  // and has correct ownership and permissions.
  if (!PairingRegistryDelegateLinux::SetupMultiProcessPairingRegistry()) {
    return false;
  }

  std::string pairings_json;
  if (!base::JSONWriter::Write(pairings, &pairings_json)) {
    std::cerr << "Failed to serialize pairings to JSON.\n";
    return false;
  }

  std::cout << "Saving pairings to multi-process registry.\n";
  // We do it in a child process as the network user to ensure the files and
  // directories are created with the correct owner and permissions.
  if (!RunWithSudo({"--" + std::string(kSaveMultiProcessPairingsSwitch)},
                   GetNetworkProcessUsername(), pairings_json)) {
    std::cerr << "Failed to save pairings to multi-process registry.\n";
    return false;
  }

  // Set multi-process config and start.
  DaemonController::SetDelegateType(
      DaemonController::DelegateType::kMultiProcess);
  auto daemon_controller = DaemonController::Create();
  base::RunLoop run_loop;
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;

  std::cout << "Disabling single-process host.\n";
  if (!DisableService("chrome-remote-desktop@" + user_name + ".service")) {
    return false;
  }

  std::cout << "Starting multi-process host.\n";
  // Note: the `consent` parameter is not used in Linux, and the
  // `usage_stats_consent` value in the host config is used instead.
  daemon_controller->SetConfigAndStart(
      std::move(*config), /*consent=*/true,
      base::BindOnce(&OnDaemonControllerDone, &result, run_loop.QuitClosure()));
  run_loop.Run();

  if (result != DaemonController::RESULT_OK) {
    std::cerr << "Failed to start multi-process host.\n";
    return false;
  }

  return true;
}

bool MigrateToSingleProcess(const base::CommandLine& command_line) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace

int MigrateHostMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(kSaveMultiProcessPairingsSwitch)) {
    return SaveMultiProcessPairingsAsNetworkUser() ? 0 : -1;
  }

  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.empty()) {
    std::cerr << "Usage: migrate_host <host_type>\n\n";
    // Internal switches are not documented here.
    HostType::PrintHostTypeHelp();
    return -1;
  }

  std::string host_type_str = args[0];
  const HostType* target_host_type = HostType::Find(host_type_str);
  if (!target_host_type) {
    std::cerr << "Unknown host type: " << host_type_str << "\n\n";
    HostType::PrintHostTypeHelp();
    return -1;
  }

  // Skip the state check when run as root and --user-name is set. This only
  // happens when the process is elevated from the user process and the state
  // check has already been performed. The state check won't work for root
  // because the single-process host is not run as root.
  if (getuid() != 0 || !command_line.HasSwitch(kUserNameSwitch)) {
    scoped_refptr<DaemonController> daemon_controller =
        DaemonController::Create();

    if (daemon_controller->GetState() != DaemonController::STATE_STARTED) {
      std::cerr << "Host migration can only be performed when the host is "
                   "already started.\n";
      return -1;
    }

    bool is_multi_process = daemon_controller->is_multi_process();
    if (is_multi_process == target_host_type->is_multi_process()) {
      // We will add more multi-process host types, but for now there is nothing
      // to do.
      std::cout << "Host is already "
                << (is_multi_process ? "multi-process" : "single-process")
                << ". Nothing to do.\n";
      return 0;
    }
  }

  bool success = false;
  if (target_host_type->is_multi_process()) {
    success = MigrateToMultiProcess(command_line);
  } else {
    success = MigrateToSingleProcess(command_line);
  }

  return success ? 0 : -1;
}

}  // namespace remoting
