// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_type_environment.h"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/host/linux/session_routing_config.h"

namespace remoting {
namespace {

// The full path is used, since this runs as root.
constexpr char kSystemctlPath[] = "/usr/bin/systemctl";

// In the multi-process host config directory.
constexpr char kHostTypeRecordFileName[] = "host_type.json";

// See kDefaultHostConfigFile, which is not used to avoid depending on
// //remoting/host.
constexpr char kMultiProcessConfigFileName[] = "host.json";

// Runs `systemctl` with `args`. Returns true if it exits with 0. Logs the
// output on failure if `log_failure` is true.
bool RunSystemctl(std::initializer_list<std::string_view> args,
                  bool log_failure) {
  base::CommandLine command_line{base::FilePath(kSystemctlPath)};
  for (std::string_view arg : args) {
    command_line.AppendArg(arg);
  }
  std::string output;
  if (base::GetAppOutputAndError(command_line, &output)) {
    return true;
  }
  if (log_failure) {
    LOG(ERROR) << "Failed to run `" << command_line.GetCommandLineString()
               << "`: " << output;
  }
  return false;
}

class HostTypeEnvironmentImpl : public HostTypeEnvironment {
 public:
  HostTypeEnvironmentImpl();
  HostTypeEnvironmentImpl(const HostTypeEnvironmentImpl&) = delete;
  HostTypeEnvironmentImpl& operator=(const HostTypeEnvironmentImpl&) = delete;
  ~HostTypeEnvironmentImpl() override;

  // HostTypeEnvironment implementation.
  std::string GetUnitActiveState(const std::string& unit) const override;
  bool IsUnitEnabled(const std::string& unit) const override;
  bool EnableAndStartUnit(const std::string& unit) override;
  bool DisableAndStopUnit(const std::string& unit) override;
  std::optional<std::vector<std::string>> ListEnabledOrActiveUnits(
      std::string_view unit_pattern) const override;
  base::FilePath GetSessionRoutingConfigPath() const override;
  base::FilePath GetHostTypeRecordPath() const override;
  bool MultiProcessConfigExists() const override;
  uid_t GetExpectedOwnerUid() const override;
};

HostTypeEnvironmentImpl::HostTypeEnvironmentImpl() = default;

HostTypeEnvironmentImpl::~HostTypeEnvironmentImpl() = default;

std::string HostTypeEnvironmentImpl::GetUnitActiveState(
    const std::string& unit) const {
  base::CommandLine command_line{base::FilePath(kSystemctlPath)};
  command_line.AppendArg("is-active");
  command_line.AppendArg(unit);
  // The state is printed even if the exit code is non-zero, which only means
  // that the unit is not active.
  std::string output;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command_line, &output, &exit_code)) {
    LOG(ERROR) << "Failed to run `" << command_line.GetCommandLineString()
               << "`";
    return std::string();
  }
  return std::string(base::TrimWhitespaceASCII(output, base::TRIM_ALL));
}

bool HostTypeEnvironmentImpl::IsUnitEnabled(const std::string& unit) const {
  return RunSystemctl({"is-enabled", unit}, /*log_failure=*/false);
}

bool HostTypeEnvironmentImpl::EnableAndStartUnit(const std::string& unit) {
  return RunSystemctl({"enable", "--now", unit}, /*log_failure=*/true);
}

bool HostTypeEnvironmentImpl::DisableAndStopUnit(const std::string& unit) {
  return RunSystemctl({"disable", "--now", unit}, /*log_failure=*/true);
}

std::optional<std::vector<std::string>>
HostTypeEnvironmentImpl::ListEnabledOrActiveUnits(
    std::string_view unit_pattern) const {
  // An enabled unit is loaded, since a target wants it, so listing all loaded
  // units that match the pattern finds the enabled units too. The list may
  // also contain units that are neither enabled nor active (e.g. units that
  // have stopped but are still loaded), so each one is checked.
  base::CommandLine command_line{base::FilePath(kSystemctlPath)};
  for (std::string_view arg :
       {"list-units", "--all", "--plain", "--no-legend", "--full", "--"}) {
    command_line.AppendArg(arg);
  }
  command_line.AppendArg(unit_pattern);
  std::string output;
  if (!base::GetAppOutput(command_line, &output)) {
    LOG(ERROR) << "Failed to run `" << command_line.GetCommandLineString()
               << "`";
    return std::nullopt;
  }
  std::vector<std::string> units;
  for (std::string_view line : base::SplitStringPiece(
           output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> fields =
        base::SplitString(line, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    if (!fields.empty() &&
        (IsUnitEnabled(fields[0]) || IsUnitActive(fields[0]))) {
      units.push_back(std::move(fields[0]));
    }
  }
  return units;
}

base::FilePath HostTypeEnvironmentImpl::GetSessionRoutingConfigPath() const {
  return SessionRoutingConfig::GetDefaultConfigFilePath();
}

base::FilePath HostTypeEnvironmentImpl::GetHostTypeRecordPath() const {
  return GetMultiProcessHostGlobalConfigDir().Append(kHostTypeRecordFileName);
}

bool HostTypeEnvironmentImpl::MultiProcessConfigExists() const {
  return base::PathExists(
      GetMultiProcessHostGlobalConfigDir().Append(kMultiProcessConfigFileName));
}

uid_t HostTypeEnvironmentImpl::GetExpectedOwnerUid() const {
  return 0;
}

}  // namespace

// static
std::unique_ptr<HostTypeEnvironment> HostTypeEnvironment::Create() {
  return std::make_unique<HostTypeEnvironmentImpl>();
}

HostTypeEnvironment::~HostTypeEnvironment() = default;

bool HostTypeEnvironment::IsUnitActive(const std::string& unit) const {
  // See
  // https://www.freedesktop.org/software/systemd/man/latest/systemctl.html
  static constexpr std::string_view kActiveStates[] = {
      "active", "activating", "reloading", "refreshing"};
  return std::ranges::contains(kActiveStates, GetUnitActiveState(unit));
}

}  // namespace remoting
