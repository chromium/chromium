// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPES_TEST_UTIL_H_
#define REMOTING_HOST_LINUX_HOST_TYPES_TEST_UTIL_H_

#include <sys/types.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

// A HostTypeEnvironment that keeps the state of systemd units in memory, and
// uses files in the directory of the session routing config file for the rest.
// The files must be owned by the effective user (see GetExpectedOwnerUid()).
class FakeHostTypeEnvironment : public HostTypeEnvironment {
 public:
  explicit FakeHostTypeEnvironment(const base::FilePath& routing_config_path);
  FakeHostTypeEnvironment(const FakeHostTypeEnvironment&) = delete;
  FakeHostTypeEnvironment& operator=(const FakeHostTypeEnvironment&) = delete;
  ~FakeHostTypeEnvironment() override;

  void SetUnitActive(const std::string& unit);
  // Units without an active state are "inactive".
  void SetUnitActiveState(const std::string& unit, std::string_view state);
  void SetUnitEnabled(const std::string& unit);
  // Makes EnableAndStartUnit() fail for `unit` without changing its state.
  void SetEnableAndStartUnitFails(const std::string& unit);
  // Makes DisableAndStopUnit() fail for `unit` without changing its state.
  void SetDisableAndStopUnitFails(const std::string& unit);
  // Makes ListEnabledOrActiveUnits() return std::nullopt.
  void SetListUnitsFails();

  // The units that EnableAndStartUnit() and DisableAndStopUnit() were called
  // for, e.g. "enable foo.service".
  const std::vector<std::string>& unit_operations() const {
    return unit_operations_;
  }

  // The path that MultiProcessConfigExists() checks.
  base::FilePath GetMultiProcessConfigPath() const;

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

 private:
  base::FilePath routing_config_path_;
  base::flat_map<std::string, std::string> unit_active_states_;
  base::flat_set<std::string> enabled_units_;
  base::flat_set<std::string> enable_failing_units_;
  base::flat_set<std::string> disable_failing_units_;
  bool list_units_fails_ = false;
  std::vector<std::string> unit_operations_;
};

// A multi-process host type that appends the calls of its validation, unit
// and state methods to `log`, e.g. "fake.SetUpState". It isn't registered, so
// HostType::GetConfigured() rejects a record that names it.
class FakeHostType : public HostType {
 public:
  FakeHostType(std::string_view name, std::vector<std::string>& log);
  FakeHostType(const FakeHostType&) = delete;
  FakeHostType& operator=(const FakeHostType&) = delete;
  ~FakeHostType() override;

  // Makes `method` (e.g. "SetUpState") fail.
  void set_fail_method(std::string_view method) { fail_method_ = method; }

  // HostType implementation.
  std::string_view name() const override;
  std::string_view description() const override;
  bool is_multi_process() const override;
  bool requires_local_user() const override;
  bool ValidateStartHostHookContext(
      const HostTypeContext& context) const override;

 protected:
  bool StartHostUnit(const HostTypeContext& context) const override;
  bool StopHostUnit(const HostTypeContext& context) const override;
  bool SetUpState(const HostTypeContext& context) const override;
  bool CleanUpState(const HostTypeContext& context) const override;

 private:
  bool Log(std::string_view method) const;

  std::string name_;
  raw_ref<std::vector<std::string>> log_;
  std::string fail_method_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPES_TEST_UTIL_H_
