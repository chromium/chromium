// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_type_migrator.h"

#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/fqdn.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/host/host_config.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/pairing_registry_delegate_linux.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting {
namespace {

constexpr char kStartHostPath[] =
    "/opt/google/chrome-remote-desktop/start-host";

constexpr char kSaveMultiProcessPairingsSwitch[] =
    "save-multi-process-pairings";
constexpr char kSaveSingleProcessHostRegistrationSwitch[] =
    "save-single-process-host-registration";
constexpr char kLoadSingleProcessHostRegistrationSwitch[] =
    "load-single-process-host-registration";
constexpr char kDeleteSingleProcessHostRegistrationSwitch[] =
    "delete-single-process-host-registration";

// The keys of the JSON dictionary that the single-process host registration
// is passed to and from the child processes as.
constexpr char kHostConfigKey[] = "host_config";
constexpr char kPairingsKey[] = "pairings";

// The maximum size of the output of a child process that is read by
// RunAsUser(), since root reads it from a child that is run as another user.
constexpr size_t kMaxChildOutputSize = 1024 * 1024;

// Runs the current program as `user_name` with `sudo -u`, passing `args`.
// Returns whether it exited with 0.
// `input`: if not empty, it is written to the child's stdin.
// `output`: if not null, it is set to what the child writes to its stdout.
//   Fails if that exceeds kMaxChildOutputSize.
bool RunAsUser(std::string_view user_name,
               base::span<const std::string_view> args,
               std::string_view input = {},
               std::string* output = nullptr) {
  CHECK(!user_name.empty());
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<std::string> full_args{"/usr/bin/sudo", "-u",
                                     std::string(user_name), "--"};
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

  bool write_ok = true;
  if (!input.empty()) {
    // Close the read end in the parent process.
    stdin_read_fd.reset();
    write_ok = base::WriteFileDescriptor(stdin_write_fd.get(), input);
    if (!write_ok) {
      PLOG(ERROR) << "Failed to write to pipe: "
                  << command_line.GetCommandLineString();
    }
    // Close the write end to signal EOF.
    stdin_write_fd.reset();
  }

  bool read_ok = true;
  if (output) {
    // Close the write end in the parent process, so that reading ends when
    // the child exits.
    stdout_write_fd.reset();

    base::ScopedFILE stdout_stream(
        base::FileToFILE(base::File(stdout_read_fd.release()), "r"));
    if (!stdout_stream ||
        !base::ReadStreamToStringWithMaxSize(stdout_stream.get(),
                                             kMaxChildOutputSize, output)) {
      LOG(ERROR) << "Failed to read the output, or it is too large: "
                 << command_line.GetCommandLineString();
      // Partial output must not be used.
      output->clear();
      read_ok = false;
    }
  }

  int exit_code = -1;
  bool exited = process.WaitForExit(&exit_code);
  if (!write_ok || !read_ok || !exited || exit_code != 0) {
    LOG(ERROR) << "Process failed with status " << exit_code << ": "
               << command_line.GetCommandLineString();
    return false;
  }
  return true;
}

bool CleanupMultiProcessHostRegistrationAsRoot() {
  CHECK_EQ(getuid(), 0u);
  std::cout << "Cleaning up multi-process host config and pairings.\n";

  bool success = true;
  base::FilePath privileged_config =
      DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath();
  if (base::PathExists(privileged_config) &&
      !base::DeleteFile(privileged_config)) {
    std::cerr << "Failed to delete privileged config file: "
              << privileged_config.value() << "\n";
    success = false;
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
  if (!pairings_value.has_value() || !pairings_value->is_list()) {
    std::cerr << "Failed to parse pairings JSON from stdin.\n";
    return false;
  }

  PairingRegistryDelegateLinux multi_pairing_delegate;
  if (!multi_pairing_delegate.DeleteAll()) {
    std::cerr
        << "Failed to clear existing pairings from multi-process registry.\n";
    return false;
  }
  for (const auto& pairing_value : pairings_value->GetList()) {
    if (!pairing_value.is_dict()) {
      continue;
    }
    if (!multi_pairing_delegate.Save(
            protocol::PairingRegistry::Pairing::CreateFromValue(
                pairing_value.GetDict()))) {
      std::cerr << "Failed to save a pairing to multi-process registry.\n";
      return false;
    }
  }

  return true;
}

base::FilePath GetSingleProcessConfigDir(const PasswdUserInfo& user_info) {
  return user_info.home_dir.Append(GetPerUserConfigRelativeDir());
}

base::FilePath GetSingleProcessConfigFile(const PasswdUserInfo& user_info) {
  return GetSingleProcessConfigDir(user_info).Append(GetHostHash() + ".json");
}

base::FilePath GetSingleProcessPairingDir(const PasswdUserInfo& user_info) {
  return GetSingleProcessConfigDir(user_info).Append(
      PairingRegistryDelegateLinux::kRegistryDirectory);
}

// Returns the passwd entry of the user that runs this process, i.e. the user
// that the child processes below are run as with sudo -u. Its home directory
// is used rather than $HOME, so that the same paths are used as by the parent
// process. Prints an error and returns std::nullopt on failure.
std::optional<PasswdUserInfo> GetCurrentUserInfo() {
  std::string user_name = GetUsername();
  auto user_info = GetPasswdUserInfo(user_name);
  if (!user_info.has_value()) {
    std::cerr << "Failed to look up the current user " << user_name << ": "
              << user_info.error() << "\n";
    return std::nullopt;
  }
  return std::move(*user_info);
}

bool SaveSingleProcessHostRegistrationAsUser() {
  CHECK_NE(getuid(), 0u);
  CHECK_NE(GetUsername(), GetNetworkProcessUsername());

  std::optional<PasswdUserInfo> user_info = GetCurrentUserInfo();
  if (!user_info.has_value()) {
    return false;
  }

  std::string json;
  if (!base::ReadStreamToString(stdin, &json)) {
    std::cerr << "Failed to read single-process host registration from "
                 "stdin.\n";
    return false;
  }

  std::optional<base::DictValue> registration_dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!registration_dict.has_value()) {
    std::cerr << "Failed to parse single-process host registration JSON from "
                 "stdin.\n";
    return false;
  }

  const base::DictValue* host_config =
      registration_dict->FindDict(kHostConfigKey);
  const base::ListValue* pairings = registration_dict->FindList(kPairingsKey);
  if (!host_config || !pairings) {
    std::cerr << "Single-process host registration JSON is missing fields.\n";
    return false;
  }

  PairingRegistryDelegateLinux user_pairing_delegate(
      GetSingleProcessPairingDir(*user_info), /*use_unprivileged_file=*/false);
  if (!user_pairing_delegate.DeleteAll()) {
    std::cerr
        << "Failed to clear existing pairings from single-process registry.\n";
    return false;
  }
  for (const auto& pairing_value : *pairings) {
    if (!pairing_value.is_dict()) {
      continue;
    }
    if (!user_pairing_delegate.Save(
            protocol::PairingRegistry::Pairing::CreateFromValue(
                pairing_value.GetDict()))) {
      std::cerr << "Failed to save a pairing to single-process registry.\n";
      return false;
    }
  }

  base::FilePath config_path = GetSingleProcessConfigFile(*user_info);
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
  if (!base::SetPosixFilePermissions(config_path, 0600)) {
    std::cerr
        << "Failed to set permissions on single-process host config file.\n";
    base::DeleteFile(config_path);
    return false;
  }

  return true;
}

// Run as the user of the single-process host, so that root doesn't follow
// symlinks in the user's home directory. Writes a JSON dictionary to stdout:
// - {"host_config": {...}, "pairings": [...]} if there is a host;
// - {} if there is no host (see ReadHostConfig()).
// If the host config exists but can't be read or parsed, nothing is written,
// the error is printed to stderr, and false is returned, so that the parent
// never mistakes a host that may be running for a deleted one.
bool LoadSingleProcessHostRegistrationAsUser() {
  CHECK_NE(getuid(), 0u);
  CHECK_NE(GetUsername(), GetNetworkProcessUsername());

  std::optional<PasswdUserInfo> user_info = GetCurrentUserInfo();
  if (!user_info.has_value()) {
    return false;
  }
  base::expected<std::optional<base::DictValue>, std::string> host_config =
      ReadHostConfig(GetSingleProcessConfigFile(*user_info));
  if (!host_config.has_value()) {
    std::cerr << host_config.error() << "\n";
    return false;
  }
  base::DictValue registration_dict;
  if (host_config->has_value()) {
    PairingRegistryDelegateLinux user_pairing_delegate(
        GetSingleProcessPairingDir(*user_info),
        /*use_unprivileged_file=*/false);
    registration_dict.Set(kHostConfigKey, std::move(**host_config));
    registration_dict.Set(kPairingsKey, user_pairing_delegate.LoadAll());
  }
  std::string registration_json;
  if (!base::JSONWriter::Write(registration_dict, &registration_json)) {
    std::cerr << "Failed to serialize single-process host registration to "
                 "JSON.\n";
    return false;
  }
  std::cout << registration_json;
  std::cout.flush();
  return std::cout.good();
}

// Run as the user of the single-process host, so that only files that the
// user can delete are deleted. Deletes the host config, then the pairings of
// the single-process host. Attempts both deletions, even if the first fails.
bool DeleteSingleProcessHostRegistrationAsUser() {
  CHECK_NE(getuid(), 0u);
  CHECK_NE(GetUsername(), GetNetworkProcessUsername());

  std::optional<PasswdUserInfo> user_info = GetCurrentUserInfo();
  if (!user_info.has_value()) {
    return false;
  }
  bool success = true;
  base::FilePath config_file = GetSingleProcessConfigFile(*user_info);
  if (!base::DeleteFile(config_file)) {
    std::cerr << "Failed to delete " << config_file.value() << "\n";
    success = false;
  }
  base::FilePath pairing_dir = GetSingleProcessPairingDir(*user_info);
  if (!base::DeletePathRecursively(pairing_dir)) {
    std::cerr << "Failed to delete " << pairing_dir.value() << "\n";
    success = false;
  }
  return success;
}

}  // namespace

std::optional<int> RunMigrationChildIfRequested(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kSaveMultiProcessPairingsSwitch)) {
    return SaveMultiProcessPairingsAsNetworkUser() ? 0 : 1;
  }
  if (command_line.HasSwitch(kSaveSingleProcessHostRegistrationSwitch)) {
    return SaveSingleProcessHostRegistrationAsUser() ? 0 : 1;
  }
  if (command_line.HasSwitch(kLoadSingleProcessHostRegistrationSwitch)) {
    return LoadSingleProcessHostRegistrationAsUser() ? 0 : 1;
  }
  if (command_line.HasSwitch(kDeleteSingleProcessHostRegistrationSwitch)) {
    return DeleteSingleProcessHostRegistrationAsUser() ? 0 : 1;
  }
  return std::nullopt;
}

base::expected<std::optional<base::DictValue>, std::string> ReadHostConfig(
    const base::FilePath& config_path) {
  // Open the file instead of checking PathExists(), which also returns false
  // if inaccessible (e.g. root-squashed NFS). Only a file known not to exist
  // means there is no host.
  base::File file(config_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    if (file.error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      return std::nullopt;
    }
    return base::unexpected(
        base::StrCat({"Failed to open ", config_path.value(), ": ",
                      base::File::ErrorToString(file.error_details())}));
  }
  file.Close();

  std::optional<base::DictValue> config = HostConfigFromJsonFile(config_path);
  if (!config.has_value()) {
    return base::unexpected(
        base::StrCat({"Failed to read ", config_path.value(),
                      ": it can't be read or isn't a JSON dictionary"}));
  }
  // A config without host ID ({}) indicates no host is registered.
  const std::string* host_id = config->FindString(kHostIdConfigPath);
  if (!host_id || host_id->empty()) {
    return std::nullopt;
  }
  return config;
}

HostTypeMigrator::HostRegistration::HostRegistration() = default;

HostTypeMigrator::HostRegistration::HostRegistration(
    base::DictValue host_config,
    base::ListValue pairings)
    : host_config(std::move(host_config)), pairings(std::move(pairings)) {}

HostTypeMigrator::HostRegistration::HostRegistration(HostRegistration&&) =
    default;

HostTypeMigrator::HostRegistration&
HostTypeMigrator::HostRegistration::operator=(HostRegistration&&) = default;

HostTypeMigrator::HostRegistration::~HostRegistration() = default;

HostTypeMigrator::HostTypeMigrator(HostTypeEnvironment& env,
                                   const HostType* current_host_type,
                                   const HostType& target_host_type,
                                   const PasswdUserInfo* local_user,
                                   bool keep_old_config)
    : env_(env),
      current_host_type_(current_host_type),
      target_host_type_(target_host_type),
      local_user_(local_user),
      keep_old_config_(keep_old_config) {}

HostTypeMigrator::~HostTypeMigrator() = default;

bool HostTypeMigrator::Run() {
  if (!current_host_type_) {
    std::cerr << "No host is set up. To set up a " << target_host_type_->name()
              << " host, run:\n";
    PrintStartHostCommand(std::cerr);
    return false;
  }

  CHECK(!local_user_name().empty() || (current_host_type_->is_multi_process() &&
                                       target_host_type_->is_multi_process()));
  base::expected<std::optional<HostRegistration>, std::string> registration =
      LoadRegistration(current_host_type_->is_multi_process());
  if (!registration.has_value()) {
    std::cerr << registration.error() << "\n";
    return false;
  }

  const bool has_registration = registration->has_value();
  const bool is_same_host_type = current_host_type_ == &*target_host_type_;

  if (!has_registration && is_same_host_type) {
    std::cerr << "The configured host type is already "
              << target_host_type_->name()
              << ", but no host is set up. To set up a "
              << target_host_type_->name() << " host, run:\n";
    PrintStartHostCommand(std::cerr);
    return false;
  }

  // Pre-record the current host type before changes so failure does not alter
  // fallback detection.
  if (!base::PathExists(env_->GetHostTypeRecordPath()) &&
      !current_host_type_->WriteHostTypeRecord(*env_)) {
    std::cerr << "Failed to record the configured host type ("
              << current_host_type_->name() << "). Nothing was changed.\n";
    return false;
  }

  if (!has_registration) {
    return SwitchHostTypeWithoutRegistration();
  }

  registration_ = std::move(**registration);
  context_.emplace(*env_, local_user_name(),
                   IsCorpHostConfig(registration_.host_config));

  if (is_same_host_type) {
    std::cout << "The host type is already " << target_host_type_->name()
              << ". Setting it up again.\n";
    if (!Repair()) {
      return false;
    }
    std::cout
        << "Successfully set up the " << target_host_type_->name()
        << " host again.\n"
        << "If the connection doesn't work as expected, try rebooting the "
           "machine.\n";
    return true;
  }

  std::cout << "Migrating from " << current_host_type_->name() << " host to "
            << target_host_type_->name() << " host.\n";
  if (!Migrate()) {
    return false;
  }
  std::cout << "Successfully migrated to " << target_host_type_->name()
            << " host.\n"
            << "If the connection doesn't work as expected, try rebooting the "
               "machine.\n";
  return true;
}

base::expected<std::optional<HostTypeMigrator::HostRegistration>, std::string>
HostTypeMigrator::LoadRegistration(bool multi_process) {
  if (multi_process) {
    base::expected<std::optional<base::DictValue>, std::string> config =
        ReadHostConfig(DaemonControllerDelegateLinuxMultiProcess::
                           GetPrivilegedConfigPath());
    if (!config.has_value()) {
      return base::unexpected(std::move(config.error()));
    }
    if (!config->has_value()) {
      return std::nullopt;
    }
    PairingRegistryDelegateLinux multi_pairing_delegate(
        PairingRegistryDelegateLinux::GetDefaultRegistryPath(),
        /*use_unprivileged_file=*/false);
    return HostRegistration(std::move(**config),
                            multi_pairing_delegate.LoadAll());
  }

  CHECK(local_user_);
  // Loaded by a child process that is run as the user, since root shouldn't
  // follow symlinks in the user's home directory. The child fails if the host
  // config exists but can't be read or parsed (it prints why), and "no host"
  // is only concluded from a successful child whose well-formed output has no
  // host config. Anything else is an error, since the host may be running.
  std::string registration_json;
  if (!RunAsUser(local_user_->username,
                 {"--" + std::string(kLoadSingleProcessHostRegistrationSwitch)},
                 /*input=*/{}, &registration_json)) {
    return base::unexpected(
        base::StrCat({"Failed to load the single-process host config and "
                      "pairings of ",
                      local_user_->username, "."}));
  }
  std::optional<base::DictValue> registration_dict =
      base::JSONReader::ReadDict(registration_json, base::JSON_PARSE_RFC);
  if (!registration_dict.has_value()) {
    return base::unexpected(
        base::StrCat({"Failed to parse the single-process host config and "
                      "pairings of ",
                      local_user_->username, "."}));
  }
  if (!registration_dict->contains(kHostConfigKey)) {
    return std::nullopt;
  }
  base::DictValue* config = registration_dict->FindDict(kHostConfigKey);
  base::ListValue* pairings = registration_dict->FindList(kPairingsKey);
  const std::string* host_id =
      config ? config->FindString(kHostIdConfigPath) : nullptr;
  if (!pairings || !host_id || host_id->empty()) {
    return base::unexpected(
        base::StrCat({"The single-process host config and pairings of ",
                      local_user_->username, " are malformed."}));
  }
  return HostRegistration(std::move(*config), std::move(*pairings));
}

bool HostTypeMigrator::SaveRegistration(bool multi_process,
                                        const HostRegistration& registration) {
  if (multi_process) {
    std::string pairings_json;
    if (!base::JSONWriter::Write(registration.pairings, &pairings_json)) {
      std::cerr << "Failed to serialize pairings to JSON.\n";
      return false;
    }

    if (!PairingRegistryDelegateLinux::SetupMultiProcessPairingRegistry()) {
      return false;
    }

    std::cout << "Saving pairings to multi-process registry.\n";
    if (!RunAsUser(GetNetworkProcessUsername(),
                   {"--" + std::string(kSaveMultiProcessPairingsSwitch)},
                   pairings_json)) {
      std::cerr << "Failed to save pairings to multi-process registry.\n";
      return false;
    }

    std::cout << "Saving multi-process host config.\n";
    // WriteConfigs() only writes config files without running the start hook
    // (unlike SetConfigAndStart()), allowing the current host to keep running
    // until its stop hook is executed in Migrate().
    if (!DaemonControllerDelegateLinuxMultiProcess::WriteConfigs(
            registration.host_config)) {
      std::cerr << "Failed to save multi-process host config.\n";
      return false;
    }
    return true;
  }

  CHECK(local_user_);
  base::DictValue registration_dict;
  registration_dict.Set(kHostConfigKey, registration.host_config.Clone());
  registration_dict.Set(kPairingsKey, registration.pairings.Clone());

  std::string registration_json;
  if (!base::JSONWriter::Write(registration_dict, &registration_json)) {
    std::cerr << "Failed to serialize single-process host registration to "
                 "JSON.\n";
    return false;
  }

  std::cout << "Saving single-process host config and pairings.\n";
  if (!RunAsUser(local_user_->username,
                 {"--" + std::string(kSaveSingleProcessHostRegistrationSwitch)},
                 registration_json)) {
    std::cerr << "Failed to save single-process host config and pairings.\n";
    return false;
  }
  return true;
}

bool HostTypeMigrator::DeleteRegistration(bool multi_process) {
  std::vector<base::FilePath> paths;
  bool success;
  if (multi_process) {
    success = CleanupMultiProcessHostRegistrationAsRoot();
    paths = {
        DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath(),
        DaemonControllerDelegateLinuxMultiProcess::GetUnprivilegedConfigPath(),
        PairingRegistryDelegateLinux::GetDefaultRegistryPath()};
  } else {
    CHECK(local_user_);
    std::cout << "Deleting old single-process config.\n";
    // Deleted by a child process that is run as the user, since root
    // shouldn't follow symlinks in the user's home directory.
    success = RunAsUser(
        local_user_->username,
        {"--" + std::string(kDeleteSingleProcessHostRegistrationSwitch)});
    paths = {GetSingleProcessConfigFile(*local_user_),
             GetSingleProcessPairingDir(*local_user_)};
  }
  if (!success) {
    std::cerr << "Failed to delete the old host config and pairings. Delete "
                 "these by hand:\n";
    for (const base::FilePath& path : paths) {
      std::cerr << "  " << path.value() << "\n";
    }
  }
  return success;
}

bool HostTypeMigrator::SwitchHostTypeWithoutRegistration() {
  // No host config exists yet to indicate whether the future host will be a
  // corp host, so pass `is_corp_host=true` to avoid rejecting domain usernames
  // (e.g. `user@domain`) before `start-host` checks with the actual host
  // config.
  HostTypeContext context(*env_, local_user_name(), /*is_corp_host=*/true);
  if (!target_host_type_->ValidateStartHostHookContext(context)) {
    std::cerr << "Cannot switch to the " << target_host_type_->name()
              << " host type. The configured host type is still "
              << current_host_type_->name() << ".\n";
    return false;
  }

  std::cout << "No host is set up. Switching the configured host type from "
            << current_host_type_->name() << " to " << target_host_type_->name()
            << ".\n";
  if (!current_host_type_->RunStopHostHook(context)) {
    std::cerr << "Failed to stop the " << current_host_type_->name()
              << " host unit or to clean up its state. The configured host "
                 "type is still "
              << current_host_type_->name() << ". To retry, run:\n";
    PrintMigrateHostCommand(*target_host_type_);
    return false;
  }
  if (!target_host_type_->WriteHostTypeRecord(*env_)) {
    std::cerr << "Failed to record " << target_host_type_->name()
              << " as the configured host type, which is still "
              << current_host_type_->name() << ". To retry, run:\n";
    PrintMigrateHostCommand(*target_host_type_);
    return false;
  }
  std::cout << "The configured host type is now " << target_host_type_->name()
            << ". To set up a " << target_host_type_->name() << " host, run:\n";
  PrintStartHostCommand(std::cout);
  return true;
}

bool HostTypeMigrator::Repair() {
  if (!target_host_type_->ValidateStartHostHookContext(*context_)) {
    std::cerr << "Cannot set up the " << target_host_type_->name()
              << " host again. The host was not changed.\n";
    return false;
  }
  // Stop the host unit first (`enable --now` won't restart an active unit,
  // and the daemon only reads the routing config on startup), without
  // running `CleanUpState()` (which would tear down state that `SetUpState()`
  // preserves, such as an active session unit).
  if (!target_host_type_->StopHostUnit(*context_) ||
      !target_host_type_->RunStartHostHook(*context_)) {
    std::cerr << "Failed to set up the " << target_host_type_->name()
              << " host again. The host may have been stopped. To retry, "
                 "run:\n";
    PrintMigrateHostCommand(*target_host_type_);
    return false;
  }
  return true;
}

bool HostTypeMigrator::Migrate() {
  if (!target_host_type_->ValidateStartHostHookContext(*context_)) {
    std::cerr << "Cannot migrate to the " << target_host_type_->name()
              << " host. The " << current_host_type_->name()
              << " host was not changed.\n";
    return false;
  }

  // Copy registration before stopping anything so failure leaves current host
  // untouched.
  if (ShouldMigrateHostRegistration() &&
      !SaveRegistration(target_host_type_->is_multi_process(), registration_)) {
    std::cerr << "Failed to copy the host config and pairings. The "
              << current_host_type_->name() << " host was not changed.\n";
    return false;
  }

  if (!current_host_type_->RunStopHostHook(*context_)) {
    std::cerr << "Failed to stop the " << current_host_type_->name()
              << " host.\n";
    PrintRetryAndRestoreCommands();
    return false;
  }

  if (!target_host_type_->RunStartHostHook(*context_)) {
    std::cerr << "Failed to start the " << target_host_type_->name()
              << " host.\n";
    PrintRetryAndRestoreCommands();
    return false;
  }

  if (ShouldMigrateHostRegistration()) {
    if (keep_old_config_) {
      std::cout << "Keeping old host config as requested.\n";
    } else if (!DeleteRegistration(current_host_type_->is_multi_process())) {
      std::cerr << "The migration to " << target_host_type_->name()
                << " succeeded, but cleaning up the old host config of the "
                << current_host_type_->name() << " host failed.\n";
      return false;
    }
  }
  return true;
}

bool HostTypeMigrator::ShouldMigrateHostRegistration() const {
  return current_host_type_->is_multi_process() !=
         target_host_type_->is_multi_process();
}

std::string_view HostTypeMigrator::local_user_name() const {
  return local_user_ ? std::string_view(local_user_->username)
                     : std::string_view();
}

void HostTypeMigrator::PrintStartHostCommand(std::ostream& stream) const {
  stream << "  " << kStartHostPath
         << " --host-type=" << target_host_type_->name() << "\n";
}

void HostTypeMigrator::PrintMigrateHostCommand(const HostType& host_type,
                                               bool keep_old_config) const {
  std::cerr << "  " << kMigrateHostPath << " " << host_type.name();
  if (!local_user_name().empty()) {
    std::cerr << " --user-name=" << local_user_name();
  }
  if (keep_old_config) {
    std::cerr << " --keep-config";
  }
  std::cerr << "\n";
}

void HostTypeMigrator::PrintRetryAndRestoreCommands() const {
  std::cerr << "The migration from " << current_host_type_->name() << " to "
            << target_host_type_->name()
            << " was not completed and has not been rolled back. To retry it, "
               "run:\n";
  PrintMigrateHostCommand(*target_host_type_, keep_old_config_);
  if (ShouldMigrateHostRegistration()) {
    std::cerr << "Retrying may keep the host config and pairings of the "
              << current_host_type_->name() << " host.\n";
  }
  std::cerr << "To restore the " << current_host_type_->name()
            << " host instead, run:\n";
  PrintMigrateHostCommand(*current_host_type_);
}

}  // namespace remoting
