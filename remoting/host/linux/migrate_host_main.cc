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
#include "base/files/file.h"
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
#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_single_process.h"

namespace remoting {

namespace {

constexpr char kUserNameSwitch[] = "user-name";
constexpr char kKeepConfigSwitch[] = "keep-config";

// Internal switches that are not meant to be used by users directly.
constexpr char kSaveMultiProcessPairingsSwitch[] =
    "save-multi-process-pairings";
constexpr char kSaveSingleProcessStateSwitch[] = "save-single-process-state";

// Runs the current program with sudo.
// `args`: The arguments to pass to the program.
// `user_name`: If not empty, the program will be run as this user (sudo -u).
// `input`: If not empty, this string will be written to the child's stdin.
// `output`: If not null, the child's stdout will be written to this string.
bool RunWithSudo(base::span<const std::string_view> args,
                 std::string_view user_name = {},
                 std::string_view input = {},
                 std::string* output = nullptr) {
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

  base::LaunchOptions options;
  options.allow_new_privs = true;

  base::ScopedFD stdin_read_fd;
  base::ScopedFD stdin_write_fd;
  if (!input.empty()) {
    if (!base::CreatePipe(&stdin_read_fd, &stdin_write_fd)) {
      PLOG(ERROR) << "Failed to create stdin pipe";
      return false;
    }
    options.fds_to_remap.emplace_back(stdin_read_fd.get(), STDIN_FILENO);
  }

  base::ScopedFD stdout_read_fd;
  base::ScopedFD stdout_write_fd;
  if (output) {
    if (!base::CreatePipe(&stdout_read_fd, &stdout_write_fd)) {
      PLOG(ERROR) << "Failed to create stdout pipe";
      return false;
    }
    options.fds_to_remap.emplace_back(stdout_write_fd.get(), STDOUT_FILENO);
  }

  base::Process process = base::LaunchProcess(command_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch process: "
               << command_line.GetCommandLineString();
    return false;
  }

  if (!input.empty()) {
    // Close the read end in the parent process.
    stdin_read_fd.reset();

    if (!base::WriteFileDescriptor(stdin_write_fd.get(), input)) {
      PLOG(ERROR) << "Failed to write to pipe: "
                  << command_line.GetCommandLineString();
      return false;
    }

    // Close the write end to signal EOF.
    stdin_write_fd.reset();
  }

  if (output) {
    // Close the write end in the parent process.
    stdout_write_fd.reset();

    base::File stdout_file(stdout_read_fd.release());
    base::ScopedFILE stdout_stream(
        base::FileToFILE(std::move(stdout_file), "r"));
    if (!stdout_stream ||
        !base::ReadStreamToString(stdout_stream.get(), output)) {
      PLOG(ERROR) << "Failed to read from pipe: "
                  << command_line.GetCommandLineString();
      return false;
    }
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

bool IsServiceActive(const std::string& unit_name) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.InitFromArgv({"systemctl", "is-active", unit_name});
  std::string output;
  return base::GetAppOutputAndError(command_line, &output);
}

bool DisableAndStopService(const std::string& unit_name) {
  std::cout << "Disabling service: " << unit_name << "\n";
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.InitFromArgv({"systemctl", "disable", "--now", unit_name});
  std::string output;
  if (!base::GetAppOutputAndError(command_line, &output)) {
    std::cerr << "Failed to disable host service (" << unit_name
              << "): " << output << "\n";
    return false;
  }
  return true;
}

bool EnableAndStartService(const std::string& unit_name) {
  std::cout << "Starting service: " << unit_name << "\n";
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.InitFromArgv({"systemctl", "enable", "--now", unit_name});
  std::string output;
  if (!base::GetAppOutputAndError(command_line, &output)) {
    std::cerr << "Failed to enable host service (" << unit_name
              << "): " << output << "\n";
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

bool CleanupMultiProcessConfigAsRoot() {
  CHECK_EQ(getuid(), 0u);

  std::cout << "Cleaning up multi-process host config.\n";

  bool success = true;
  base::FilePath privileged_config =
      DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath();
  if (base::PathExists(privileged_config) &&
      !base::DeleteFile(privileged_config)) {
    std::cerr << "Failed to delete privileged config file: "
              << privileged_config.value() << "\n";
    success = false;
    // We continue even if the privileged config can't be deleted, to delete
    // other files, since the single-process host has already started.
  }

  base::FilePath unprivileged_config =
      DaemonControllerDelegateLinuxMultiProcess::GetUnprivilegedConfigPath();
  if (base::PathExists(unprivileged_config) &&
      !base::DeleteFile(unprivileged_config)) {
    std::cerr << "Failed to delete unprivileged config file: "
              << unprivileged_config.value() << "\n";
    success = false;
  }

  base::FilePath pairing_dir =
      PairingRegistryDelegateLinux::GetDefaultRegistryPath();
  if (base::PathExists(pairing_dir) &&
      !base::DeletePathRecursively(pairing_dir)) {
    std::cerr << "Failed to delete pairing directory: " << pairing_dir.value()
              << "\n";
    success = false;
  }

  return success;
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

bool SaveSingleProcessStateAsUser() {
  std::string json;
  if (!base::ReadStreamToString(stdin, &json)) {
    std::cerr << "Failed to read single-process state from stdin.\n";
    return false;
  }

  std::optional<base::Value> state_value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!state_value || !state_value->is_dict()) {
    std::cerr << "Failed to parse single-process state JSON from stdin.\n";
    return false;
  }

  const base::DictValue& state_dict = state_value->GetDict();
  const base::DictValue* host_config = state_dict.FindDict("host_config");
  const base::ListValue* pairings = state_dict.FindList("pairings");
  if (!host_config || !pairings) {
    std::cerr << "Single-process state JSON is missing fields.\n";
    return false;
  }

  PairingRegistryDelegateLinux user_pairing_delegate;
  for (const auto& pairing_value : *pairings) {
    if (!pairing_value.is_dict()) {
      continue;
    }
    if (!user_pairing_delegate.Save(
            protocol::PairingRegistry::Pairing::CreateFromValue(
                pairing_value.GetDict()))) {
      std::cerr << "Failed to save a pairing to single-process registry.\n";
    }
  }

  base::FilePath config_path =
      DaemonControllerDelegateLinuxSingleProcess::GetConfigPath();
  base::FilePath config_dir = config_path.DirName();
  if (!base::DirectoryExists(config_dir)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(config_dir, &error)) {
      std::cerr << "Failed to create user config directory: "
                << base::File::ErrorToString(error) << "\n";
      return false;
    }
  }

  if (!HostConfigToJsonFile(host_config->Clone(), config_path)) {
    std::cerr << "Failed to write single-process host config file.\n";
    return false;
  }

  return true;
}

bool MigrateToMultiProcess(const base::CommandLine& command_line,
                           const PasswdUserInfo& user_info) {
  CHECK_EQ(getuid(), 0u);

  if (IsServiceActive("chrome-remote-desktop.service")) {
    std::cout << "Host is already multi-process. Nothing to do.\n";
    return true;
  }

  base::FilePath user_config_dir =
      user_info.home_dir.Append(GetPerUserConfigRelativeDir());
  base::FilePath config_file = user_config_dir.Append(GetHostHash() + ".json");

  if (!base::PathExists(config_file)) {
    std::cerr << "Host migration can only be performed when the host is "
                 "already started.\n";
    return false;
  }

  std::cout << "Migrating from single-process host to multi-process host.\n";

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

  auto daemon_controller = DaemonController::Create();
  base::RunLoop run_loop;
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;

  std::cout << "Disabling single-process host.\n";
  if (!DisableAndStopService("chrome-remote-desktop@" + user_info.username +
                             ".service")) {
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

  if (command_line.HasSwitch(kKeepConfigSwitch)) {
    std::cout << "Keeping old single-process config as requested.\n";
  } else {
    std::cout << "Deleting old single-process config.\n";
    if (base::PathExists(user_pairing_dir) &&
        !base::DeletePathRecursively(user_pairing_dir)) {
      std::cerr << "Failed to delete old pairing directory.\n";
      return false;
    }
    if (base::PathExists(config_file) && !base::DeleteFile(config_file)) {
      std::cerr << "Failed to delete old host config file.\n";
      return false;
    }
  }

  std::cout << "Successfully migrated to multi-process host.\n";
  return true;
}

bool MigrateToSingleProcess(const base::CommandLine& command_line,
                            const PasswdUserInfo& user_info) {
  CHECK_EQ(getuid(), 0u);

  if (IsServiceActive("chrome-remote-desktop@" + user_info.username +
                      ".service")) {
    std::cout << "Host is already single-process. Nothing to do.\n";
    return true;
  }

  base::FilePath config_path =
      DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath();
  if (!base::PathExists(config_path)) {
    std::cerr << "Host migration can only be performed when the host is "
                 "already started.\n";
    return false;
  }

  std::cout << "Migrating from multi-process host to single-process host.\n";

  std::optional<base::DictValue> host_config =
      HostConfigFromJsonFile(config_path);
  if (!host_config) {
    std::cerr << "Failed to read multi-process host config.\n";
    return false;
  }

  PairingRegistryDelegateLinux multi_pairing_delegate(
      PairingRegistryDelegateLinux::GetDefaultRegistryPath(),
      /*use_unprivileged_file=*/false);
  base::ListValue pairings = multi_pairing_delegate.LoadAll();

  base::DictValue state_dict;
  state_dict.Set("host_config", std::move(*host_config));
  state_dict.Set("pairings", std::move(pairings));

  std::string state_json;
  if (!base::JSONWriter::Write(state_dict, &state_json)) {
    std::cerr << "Failed to serialize single-process state to JSON.\n";
    return false;
  }

  // We can't use `DaemonController` here because the single-process host
  // delegate only works when the process is run as the user.
  std::cout << "Saving state to single-process registry.\n";
  if (!RunWithSudo({"--" + std::string(kSaveSingleProcessStateSwitch)},
                   user_info.username, state_json)) {
    std::cerr << "Failed to save state to single-process registry.\n";
    return false;
  }

  std::cout << "Disabling multi-process host.\n";
  if (!DisableAndStopService("chrome-remote-desktop.service")) {
    return false;
  }

  if (!EnableAndStartService("chrome-remote-desktop@" + user_info.username +
                             ".service")) {
    std::cerr << "Failed to start single-process host.\n";
    return false;
  }

  if (command_line.HasSwitch(kKeepConfigSwitch)) {
    std::cout << "Keeping multi-process host state as requested.\n";
  } else {
    if (!CleanupMultiProcessConfigAsRoot()) {
      std::cerr << "Failed to clean up multi-process host state.\n";
      return false;
    }
  }

  std::cout << "Successfully migrated to single-process host.\n";
  return true;
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

  if (command_line.HasSwitch(kSaveSingleProcessStateSwitch)) {
    return SaveSingleProcessStateAsUser() ? 0 : -1;
  }

  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.empty()) {
    std::cerr << "Usage: migrate_host <host_type> [--user-name=<username>] "
                 "[--keep-config]\n\n";
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

  if (getuid() != 0) {
    std::string user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
    if (user_name.empty()) {
      user_name = GetUsername();
    }

    base::CommandLine sudo_command_line(base::CommandLine::NO_PROGRAM);
    std::vector<std::string> full_args{
        "/usr/bin/sudo",
        "-k",
        "--",
        command_line.GetProgram().value(),
        host_type_str,
        base::StringPrintf("--%s=%s", kUserNameSwitch, user_name.c_str())};

    if (command_line.HasSwitch(kKeepConfigSwitch)) {
      full_args.push_back(base::StringPrintf("--%s", kKeepConfigSwitch));
    }

    sudo_command_line.InitFromArgv(full_args);

    base::LaunchOptions options;
    options.allow_new_privs = true;

    int exit_code = -1;
    auto process = base::LaunchProcess(sudo_command_line, options);
    if (!process.IsValid() || !process.WaitForExit(&exit_code)) {
      std::cerr << "Failed to launch elevated migrate_host process.\n";
      return -1;
    }

    return exit_code;
  }

  // Code below is run as root.
  std::string user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
  if (user_name.empty()) {
    std::cerr << "--" << kUserNameSwitch
              << " must be provided when run as root.\n";
    return -1;
  }

  auto user_info = GetPasswdUserInfo(user_name);
  if (!user_info.has_value()) {
    std::cerr << "Failed to look up user: " << user_name << ": "
              << user_info.error() << "\n";
    return -1;
  }

  bool success = false;
  DaemonController::SetHostType(target_host_type);
  if (target_host_type->is_multi_process()) {
    success = MigrateToMultiProcess(command_line, *user_info);
  } else {
    success = MigrateToSingleProcess(command_line, *user_info);
  }

  return success ? 0 : -1;
}

}  // namespace remoting
