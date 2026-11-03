// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_type_environment.h"

#include <initializer_list>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "remoting/host/linux/session_routing_config.h"

namespace remoting {
namespace {

// Runs `systemctl` with `args`. Returns true if it exits with 0. Logs the
// output on failure if `log_failure` is true.
bool RunSystemctl(std::initializer_list<std::string_view> args,
                  bool log_failure) {
  base::CommandLine command_line(base::FilePath("systemctl"));
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
  bool IsUnitActive(const std::string& unit) const override;
  bool IsUnitEnabled(const std::string& unit) const override;
  bool EnableAndStartUnit(const std::string& unit) override;
  bool DisableAndStopUnit(const std::string& unit) override;
  base::FilePath GetSessionRoutingConfigPath() const override;
  uid_t GetExpectedOwnerUid() const override;
};

HostTypeEnvironmentImpl::HostTypeEnvironmentImpl() = default;

HostTypeEnvironmentImpl::~HostTypeEnvironmentImpl() = default;

bool HostTypeEnvironmentImpl::IsUnitActive(const std::string& unit) const {
  // A non-zero exit code means that the unit is not active, which is not an
  // error.
  return RunSystemctl({"is-active", unit}, /*log_failure=*/false);
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

base::FilePath HostTypeEnvironmentImpl::GetSessionRoutingConfigPath() const {
  return SessionRoutingConfig::GetDefaultConfigFilePath();
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

}  // namespace remoting
