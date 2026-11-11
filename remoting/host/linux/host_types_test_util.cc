// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types_test_util.h"

#include <unistd.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"

namespace remoting {

// FakeHostTypeEnvironment

FakeHostTypeEnvironment::FakeHostTypeEnvironment(
    const base::FilePath& routing_config_path)
    : routing_config_path_(routing_config_path) {}

FakeHostTypeEnvironment::~FakeHostTypeEnvironment() = default;

void FakeHostTypeEnvironment::SetUnitActive(const std::string& unit) {
  SetUnitActiveState(unit, "active");
}

void FakeHostTypeEnvironment::SetUnitActiveState(const std::string& unit,
                                                 std::string_view state) {
  unit_active_states_.insert_or_assign(unit, std::string(state));
}

void FakeHostTypeEnvironment::SetUnitEnabled(const std::string& unit) {
  enabled_units_.insert(unit);
}

void FakeHostTypeEnvironment::SetEnableAndStartUnitFails(
    const std::string& unit) {
  enable_failing_units_.insert(unit);
}

void FakeHostTypeEnvironment::SetDisableAndStopUnitFails(
    const std::string& unit) {
  disable_failing_units_.insert(unit);
}

void FakeHostTypeEnvironment::SetListUnitsFails() {
  list_units_fails_ = true;
}

base::FilePath FakeHostTypeEnvironment::GetMultiProcessConfigPath() const {
  return routing_config_path_.DirName().Append("host.json");
}

std::string FakeHostTypeEnvironment::GetUnitActiveState(
    const std::string& unit) const {
  auto it = unit_active_states_.find(unit);
  return it == unit_active_states_.end() ? "inactive" : it->second;
}

bool FakeHostTypeEnvironment::IsUnitEnabled(const std::string& unit) const {
  return enabled_units_.contains(unit);
}

bool FakeHostTypeEnvironment::EnableAndStartUnit(const std::string& unit) {
  unit_operations_.push_back("enable " + unit);
  if (enable_failing_units_.contains(unit)) {
    return false;
  }
  enabled_units_.insert(unit);
  SetUnitActive(unit);
  return true;
}

bool FakeHostTypeEnvironment::DisableAndStopUnit(const std::string& unit) {
  unit_operations_.push_back("disable " + unit);
  if (disable_failing_units_.contains(unit)) {
    return false;
  }
  enabled_units_.erase(unit);
  unit_active_states_.erase(unit);
  return true;
}

std::optional<std::vector<std::string>>
FakeHostTypeEnvironment::ListEnabledOrActiveUnits(
    std::string_view unit_pattern) const {
  if (list_units_fails_) {
    return std::nullopt;
  }
  base::flat_set<std::string> units;
  for (const auto& [unit, state] : unit_active_states_) {
    if (base::MatchPattern(unit, unit_pattern) && IsUnitActive(unit)) {
      units.insert(unit);
    }
  }
  for (const std::string& unit : enabled_units_) {
    if (base::MatchPattern(unit, unit_pattern)) {
      units.insert(unit);
    }
  }
  return std::move(units).extract();
}

base::FilePath FakeHostTypeEnvironment::GetSessionRoutingConfigPath() const {
  return routing_config_path_;
}

base::FilePath FakeHostTypeEnvironment::GetHostTypeRecordPath() const {
  return routing_config_path_.DirName().Append("host_type.json");
}

bool FakeHostTypeEnvironment::MultiProcessConfigExists() const {
  return base::PathExists(GetMultiProcessConfigPath());
}

uid_t FakeHostTypeEnvironment::GetExpectedOwnerUid() const {
  return geteuid();
}

// FakeHostType

FakeHostType::FakeHostType(std::string_view name, std::vector<std::string>& log)
    : name_(name), log_(log) {}

FakeHostType::~FakeHostType() = default;

std::string_view FakeHostType::name() const {
  return name_;
}

std::string_view FakeHostType::description() const {
  return "Fake host type.";
}

bool FakeHostType::is_multi_process() const {
  return true;
}

bool FakeHostType::requires_local_user() const {
  return false;
}

bool FakeHostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  return Log("ValidateStartHostHookContext");
}

bool FakeHostType::StartHostUnit(const HostTypeContext& context) const {
  return Log("StartHostUnit");
}

bool FakeHostType::StopHostUnit(const HostTypeContext& context) const {
  return Log("StopHostUnit");
}

bool FakeHostType::SetUpState(const HostTypeContext& context) const {
  return Log("SetUpState");
}

bool FakeHostType::CleanUpState(const HostTypeContext& context) const {
  return Log("CleanUpState");
}

bool FakeHostType::Log(std::string_view method) const {
  log_->push_back(base::StrCat({name_, ".", method}));
  return method != fail_method_;
}

}  // namespace remoting
