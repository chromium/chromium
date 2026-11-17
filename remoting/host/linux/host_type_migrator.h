// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_HOST_TYPE_MIGRATOR_H_
#define REMOTING_HOST_LINUX_HOST_TYPE_MIGRATOR_H_

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "remoting/host/linux/host_types.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace remoting {

class HostTypeEnvironment;
struct PasswdUserInfo;

// Runs the migration child helper requested by internal switches in
// `command_line` and returns its exit code, or std::nullopt.
std::optional<int> RunMigrationChildIfRequested(
    const base::CommandLine& command_line);

// Reads the host config at `config_path`. Returns std::nullopt if the file does
// not exist or has no host ID. Returns an error message if the file cannot be
// read or parsed. Exposed for unit testing.
base::expected<std::optional<base::DictValue>, std::string> ReadHostConfig(
    const base::FilePath& config_path);

// Migrates between configured host types, or repairs the current host type if
// it is already the target.
class HostTypeMigrator {
 public:
  HostTypeMigrator(HostTypeEnvironment& env,
                   const HostType* current_host_type,
                   const HostType& target_host_type,
                   const PasswdUserInfo* local_user,
                   bool keep_old_config);
  HostTypeMigrator(const HostTypeMigrator&) = delete;
  HostTypeMigrator& operator=(const HostTypeMigrator&) = delete;
  virtual ~HostTypeMigrator();

  // Runs the migration or repair. Returns true on success.
  bool Run();

 protected:
  struct HostRegistration {
    HostRegistration();
    HostRegistration(base::DictValue host_config, base::ListValue pairings);
    HostRegistration(HostRegistration&&);
    HostRegistration& operator=(HostRegistration&&);
    ~HostRegistration();

    base::DictValue host_config;
    base::ListValue pairings;
  };

  // Loads the host config and pairings of the given process model.
  virtual base::expected<std::optional<HostRegistration>, std::string>
  LoadRegistration(bool multi_process);

  // Writes `registration` to the given process model.
  virtual bool SaveRegistration(bool multi_process,
                                const HostRegistration& registration);

  // Deletes the host config and pairings of the given process model.
  virtual bool DeleteRegistration(bool multi_process);

 private:
  bool SwitchHostTypeWithoutRegistration();
  bool Repair();
  bool Migrate();

  bool ShouldMigrateHostRegistration() const;
  std::string_view local_user_name() const;

  void PrintStartHostCommand(std::ostream& stream) const;
  void PrintMigrateHostCommand(const HostType& host_type,
                               bool keep_old_config = false) const;
  void PrintRetryAndRestoreCommands() const;

  const raw_ref<HostTypeEnvironment> env_;
  const raw_ptr<const HostType> current_host_type_;
  const raw_ref<const HostType> target_host_type_;
  const raw_ptr<const PasswdUserInfo> local_user_;
  const bool keep_old_config_;

  HostRegistration registration_;
  std::optional<HostTypeContext> context_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_HOST_TYPE_MIGRATOR_H_
