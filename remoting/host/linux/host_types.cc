// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/secure_file_reader.h"

namespace remoting {
namespace {

constexpr char kSingleProcessUnitPrefix[] = "chrome-remote-desktop@";

// Printed when stopping an active single-process host unit.
constexpr char kSingleProcessUnitStopWarning[] =
    " Unsaved work in its desktop session will be lost.";

// The key of the host type name in the host type record.
constexpr char kHostTypeRecordKey[] = "hostType";

// The host type record is much smaller than this.
constexpr size_t kMaxHostTypeRecordSizeBytes = 4 * 1024;

std::string GetSingleProcessUnit(std::string_view local_user_name) {
  return base::StrCat({kSingleProcessUnitPrefix, local_user_name, ".service"});
}

// Returns the error message for an invalid host type record at `path`.
// `reason` describes what is wrong with it.
std::string InvalidHostTypeRecordError(const base::FilePath& path,
                                       std::string_view reason) {
  return base::StrCat({"The host type record '", path.value(), "' is invalid. ",
                       reason, "\nFix or delete the file, then try again."});
}

// Renames the session routing config file at `path` to `<path>.bak`,
// overwriting any existing backup, and prints the path of the backup. Does
// nothing if the file doesn't exist. Returns false on failure.
bool BackUpSessionRoutingConfig(const base::FilePath& path) {
  base::FilePath backup_path = path.AddExtension("bak");
  // rename() replaces `backup_path` atomically, and moves `path` itself if it
  // is a symlink.
  base::File::Error error;
  if (!base::ReplaceFile(path, backup_path, &error)) {
    if (error == base::File::FILE_ERROR_NOT_FOUND) {
      return true;
    }
    std::cerr << "Failed to move " << path << " to " << backup_path << ": "
              << base::File::ErrorToString(error) << "\n";
    return false;
  }
  std::cout << "Moved the session routing config file to " << backup_path
            << "\n";
  return true;
}

// Stops and disables `unit` if it is enabled or active. Prints `warning` (may
// be empty) when stopping an active unit. Returns false on failure.
bool StopUnit(HostTypeEnvironment& env,
              const std::string& unit,
              std::string_view warning) {
  bool active = env.IsUnitActive(unit);
  if (!active && !env.IsUnitEnabled(unit)) {
    return true;
  }
  if (active) {
    std::cout << "Stopping " << unit << "." << warning << "\n";
  }
  if (!env.DisableAndStopUnit(unit)) {
    std::cerr << "Failed to stop " << unit << ".\n";
    return false;
  }
  return true;
}

// Enables and starts `unit`. Returns false on failure.
bool StartUnit(HostTypeEnvironment& env, const std::string& unit) {
  std::cout << "Starting " << unit << ".\n";
  if (!env.EnableAndStartUnit(unit)) {
    std::cerr << "Failed to start " << unit << ".\n";
    return false;
  }
  return true;
}

// Stops and disables the single-process host unit of
// `context.local_user_name` if it is enabled or active. Does nothing if
// `context.local_user_name` is empty (see CheckLocalUserForHostTypeSetup()).
// Returns false on failure.
bool StopSingleProcessUnit(const HostTypeContext& context) {
  if (context.local_user_name.empty()) {
    return true;
  }
  return StopUnit(*context.env, GetSingleProcessUnit(context.local_user_name),
                  kSingleProcessUnitStopWarning);
}

// Stops and disables the single-process host units of all users that are
// enabled or active. Returns false on failure, including if the units can't
// be listed.
bool StopAllSingleProcessUnits(HostTypeEnvironment& env) {
  std::string pattern = GetSingleProcessUnit("*");
  std::optional<std::vector<std::string>> units =
      env.ListEnabledOrActiveUnits(pattern);
  if (!units.has_value()) {
    std::cerr << "Failed to list the units matching " << pattern << ".\n";
    return false;
  }
  for (const std::string& unit : *units) {
    if (!StopUnit(env, unit, kSingleProcessUnitStopWarning)) {
      return false;
    }
  }
  return true;
}

// Stops and disables the multi-process host unit if it is enabled or active.
// Returns false on failure.
bool StopMultiProcessUnit(HostTypeEnvironment& env) {
  return StopUnit(env, kMultiProcessHostUnit, /*warning=*/"");
}

// Base class of the single-process host type, whose host unit is
// `chrome-remote-desktop@<user>.service`. It has no other state. Its
// SetUpState() stops and disables the multi-process host unit if it is
// enabled or active, e.g. after the multi-process host was stopped without
// its stop hook, since both host units can't be used at the same time.
class SingleProcessHostType : public HostType {
 public:
  ~SingleProcessHostType() override;
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
};

// Base class of the multi-process host types, whose host unit is
// `chrome-remote-desktop.service`.
class MultiProcessHostType : public HostType {
 public:
  ~MultiProcessHostType() override;
  bool is_multi_process() const override;

 protected:
  bool StartHostUnit(const HostTypeContext& context) const override;
  bool StopHostUnit(const HostTypeContext& context) const override;

  // HostType implementation. Stops and disables the single-process host units
  // of all users that are enabled or active, e.g. after the single-process
  // host was stopped without its stop hook, since both host units can't be
  // used at the same time. Subclasses that override SetUpState() must call
  // this first.
  bool SetUpState(const HostTypeContext& context) const override;

  // HostType implementation. Subclasses that override CleanUpState() must
  // call this first. Backs up the session routing config file.
  bool CleanUpState(const HostTypeContext& context) const override;
};

// Multi-process host whose sessions are managed by GDM, which is the case if
// there is no session routing config file.
class GdmManagedHostType : public MultiProcessHostType {
 public:
  ~GdmManagedHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool requires_local_user() const override;

 protected:
  bool SetUpState(const HostTypeContext& context) const override;
};

// SingleProcessHostType

SingleProcessHostType::~SingleProcessHostType() = default;

std::string_view SingleProcessHostType::name() const {
  return "single-process";
}

std::string_view SingleProcessHostType::description() const {
  return "Single-process host (legacy). This host type is run as the user, "
         "and supports X11.";
}

bool SingleProcessHostType::is_multi_process() const {
  return false;
}

bool SingleProcessHostType::requires_local_user() const {
  return true;
}

bool SingleProcessHostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  if (context.local_user_name.empty()) {
    std::cerr << "Cannot start the single-process host, since the local user "
                 "is unknown.\n";
    return false;
  }
  if (!IsUserNameSafeForUnitName(context.local_user_name)) {
    std::cerr << "Cannot start the single-process host, since the username '"
              << context.local_user_name << "' is not supported.\n";
    return false;
  }
  return true;
}

bool SingleProcessHostType::StartHostUnit(
    const HostTypeContext& context) const {
  if (context.local_user_name.empty()) {
    std::cerr << "Cannot start the single-process host, since the local user "
                 "is unknown.\n";
    return false;
  }
  return StartUnit(*context.env, GetSingleProcessUnit(context.local_user_name));
}

bool SingleProcessHostType::StopHostUnit(const HostTypeContext& context) const {
  return StopSingleProcessUnit(context);
}

bool SingleProcessHostType::SetUpState(const HostTypeContext& context) const {
  return StopMultiProcessUnit(*context.env);
}

// MultiProcessHostType

MultiProcessHostType::~MultiProcessHostType() = default;

bool MultiProcessHostType::is_multi_process() const {
  return true;
}

bool MultiProcessHostType::StartHostUnit(const HostTypeContext& context) const {
  return StartUnit(*context.env, kMultiProcessHostUnit);
}

bool MultiProcessHostType::StopHostUnit(const HostTypeContext& context) const {
  return StopMultiProcessUnit(*context.env);
}

bool MultiProcessHostType::SetUpState(const HostTypeContext& context) const {
  // All users' units are stopped, so that the local user isn't needed, e.g.
  // after a single-process host was set up for a different user.
  return StopAllSingleProcessUnits(*context.env);
}

bool MultiProcessHostType::CleanUpState(const HostTypeContext& context) const {
  // Kept as a backup, since the file may have been edited by hand.
  return BackUpSessionRoutingConfig(context.env->GetSessionRoutingConfigPath());
}

// GdmManagedHostType

GdmManagedHostType::~GdmManagedHostType() = default;

std::string_view GdmManagedHostType::name() const {
  return "gdm-managed";
}

std::string_view GdmManagedHostType::description() const {
  return "GDM-managed multi-process host. This host type only supports GNOME "
         "49+ in Wayland mode. You will be presented with the login screen "
         "the first time you connect. Note that it doesn't support running "
         "multiple hosts for different users.";
}

bool GdmManagedHostType::requires_local_user() const {
  return false;
}

bool GdmManagedHostType::SetUpState(const HostTypeContext& context) const {
  if (!MultiProcessHostType::SetUpState(context)) {
    return false;
  }
  // GDM only manages the sessions if the session routing config file doesn't
  // exist. An existing file may have been edited by hand, so it is kept as a
  // backup.
  return BackUpSessionRoutingConfig(context.env->GetSessionRoutingConfigPath());
}

const SingleProcessHostType& GetSingleProcessHostType() {
  static const base::NoDestructor<SingleProcessHostType> instance;
  return *instance;
}

const GdmManagedHostType& GetGdmManagedHostType() {
  static const base::NoDestructor<GdmManagedHostType> instance;
  return *instance;
}

}  // namespace

HostTypeContext::HostTypeContext(HostTypeEnvironment& env,
                                 std::string_view local_user_name,
                                 bool is_corp_host)
    : env(env), local_user_name(local_user_name), is_corp_host(is_corp_host) {}

HostTypeContext::~HostTypeContext() = default;

// static
const base::flat_map<std::string_view, const HostType*>&
HostType::GetHostTypes() {
  static const base::NoDestructor<
      base::flat_map<std::string_view, const HostType*>>
      kHostTypes({
          {GetSingleProcessHostType().name(), &GetSingleProcessHostType()},
          {GetGdmManagedHostType().name(), &GetGdmManagedHostType()},
      });
  return *kHostTypes;
}

// static
const HostType& HostType::GetDefaultHostType() {
  return GetSingleProcessHostType();
}

// static
const HostType* HostType::Find(std::string_view name) {
  const auto& host_types = GetHostTypes();
  auto it = host_types.find(name);
  return it == host_types.end() ? nullptr : it->second;
}

// static
void HostType::PrintHostTypeHelp() {
  std::cerr << "Supported host types:";
  for (const auto& [name, type] : GetHostTypes()) {
    std::cerr << "\n\n"
              << "  " << name
              << (&GetDefaultHostType() == type ? " (default)" : "") << "\n    "
              << type->description();
  }
  std::cerr << "\n";
}

// static
base::expected<const HostType*, std::string> HostType::GetConfigured(
    const HostTypeEnvironment& env,
    std::string_view local_user_name) {
  base::FilePath path = env.GetHostTypeRecordPath();
  // The record decides which host type root sets up, so it must only be
  // writable by root.
  auto content = ReadSecureFile(path, env.GetExpectedOwnerUid(),
                                kMaxHostTypeRecordSizeBytes);
  if (!content.has_value()) {
    return base::unexpected(InvalidHostTypeRecordError(
        path, base::StrCat({"Failed to read it securely: ",
                            content.error().ToString()})));
  }
  if (!content->has_value()) {
    return base::ok(GetFallbackConfigured(env, local_user_name));
  }
  std::optional<base::Value> json =
      base::JSONReader::Read(**content, base::JSON_PARSE_RFC);
  if (!json.has_value() || !json->is_dict()) {
    return base::unexpected(
        InvalidHostTypeRecordError(path, "It is not a JSON dictionary."));
  }
  const std::string* name = json->GetDict().FindString(kHostTypeRecordKey);
  if (!name) {
    return base::unexpected(InvalidHostTypeRecordError(
        path, base::StrCat(
                  {"It has no string \"", kHostTypeRecordKey, "\" field."})));
  }
  const HostType* host_type = Find(*name);
  if (!host_type) {
    return base::unexpected(InvalidHostTypeRecordError(
        path, base::StrCat({"Unknown host type '", *name, "'."})));
  }
  return base::ok(host_type);
}

// static
const HostType* HostType::GetFallbackConfigured(
    const HostTypeEnvironment& env,
    std::string_view local_user_name) {
  // TODO(crbug.com/493980875): Return the host type that uses the session
  // routing config file if it exists, once there is one.
  const HostType& multi_process_host_type = GetGdmManagedHostType();
  bool has_multi_process_config = env.MultiProcessConfigExists();

  // 1. The multi-process host config exists and the multi-process host unit
  //    is enabled.
  if (has_multi_process_config && env.IsUnitEnabled(kMultiProcessHostUnit)) {
    return &multi_process_host_type;
  }

  // 2. The single-process host unit of `local_user_name` is enabled. It stays
  //    enabled when the single-process host is stopped or deleted, and takes
  //    precedence over a multi-process host config that is left over from
  //    `migrate-host --keep-config`.
  if (IsUserNameSafeForUnitName(local_user_name) &&
      env.IsUnitEnabled(GetSingleProcessUnit(local_user_name))) {
    return &GetSingleProcessHostType();
  }

  // 3. Neither host unit is enabled. If the multi-process host config exists,
  //    this is a stopped multi-process host: DaemonController::Stop() disables
  //    the host unit and keeps the host config.
  if (has_multi_process_config) {
    return &multi_process_host_type;
  }

  return nullptr;
}

bool HostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  return true;
}

bool HostType::WriteHostTypeRecord(const HostTypeEnvironment& env) const {
  base::FilePath path = env.GetHostTypeRecordPath();
  base::FilePath directory = path.DirName();
  // The directory is also where the multi-process host config is written,
  // which sets the same mode.
  if (!base::PathExists(directory)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(directory, &error)) {
      std::cerr << "Failed to create " << directory << ": "
                << base::File::ErrorToString(error) << "\n";
      return false;
    }
    if (!base::SetPosixFilePermissions(directory, 0755)) {
      std::cerr << "Failed to set the permissions of " << directory << ".\n";
      return false;
    }
  }
  // Never write a record that GetConfigured() would refuse to read.
  auto directory_check =
      CheckSecureDirectory(directory, env.GetExpectedOwnerUid());
  if (!directory_check.has_value()) {
    std::cerr << "Not writing the host type record " << path << ": "
              << directory_check.error() << "\n";
    return false;
  }

  base::DictValue dict;
  dict.Set(kHostTypeRecordKey, name());
  std::optional<std::string> json = base::WriteJson(dict);
  CHECK(json.has_value());
  // WriteFileAtomically() writes to a temporary file created with mode 0600 in
  // the same directory, then renames it over `path` (replacing rather than
  // following any symlink there). Operating on paths here and in
  // SetPosixFilePermissions() (which follows symlinks) is fine since the
  // directory has just been verified to be only writable by its owner.
  if (!base::ImportantFileWriter::WriteFileAtomically(path, *json)) {
    std::cerr << "Failed to write the host type record " << path << ".\n";
    return false;
  }
  // Readable by everyone, e.g. by the native messaging host.
  if (!base::SetPosixFilePermissions(path, 0644)) {
    std::cerr << "Failed to set the permissions of " << path << ".\n";
    return false;
  }
  return true;
}

bool HostType::SetUpAndStartHost(const HostTypeContext& context,
                                 bool keep_existing_setup) const {
  if (keep_existing_setup) {
    std::cout << "Keeping the existing " << name() << " setup.\n";
    return StartHostUnit(context);
  }
  if (!RunStartHostHook(context)) {
    std::cerr << "Failed to start the " << name() << " host.\n";
    return false;
  }
  return true;
}

bool HostType::RunStartHostHook(const HostTypeContext& context) const {
  return ValidateStartHostHookContext(context) &&
         WriteHostTypeRecord(*context.env) && SetUpState(context) &&
         StartHostUnit(context);
}

bool HostType::RunStopHostHook(const HostTypeContext& context) const {
  return StopHostUnit(context) && CleanUpState(context);
}

bool HostType::SetUpState(const HostTypeContext& context) const {
  return true;
}

bool HostType::CleanUpState(const HostTypeContext& context) const {
  return true;
}

bool IsUserNameSafeForUnitName(std::string_view user_name) {
  // systemd unit names are at most 255 characters long, and only allow ASCII
  // letters, digits, ":-_.\" and '@'. '\' (systemd's escape character) is
  // excluded. '@' is allowed in the instance part, so SSSD names such as
  // "user@domain" work. The length bound leaves room for the unit's prefix
  // and suffix.
  constexpr size_t kMaxLength = 200;
  constexpr std::string_view kAllowedPunctuation = "._@:-";
  return !user_name.empty() && user_name.size() <= kMaxLength &&
         user_name.front() != '-' &&
         std::ranges::all_of(user_name, [&](char c) {
           return base::IsAsciiAlphaNumeric(c) ||
                  kAllowedPunctuation.find(c) != std::string_view::npos;
         });
}

base::expected<void, std::string> CheckLocalUserForHostTypeSetup(
    const HostType& target_host_type,
    const HostType* current_host_type,
    std::string_view local_user_name) {
  if (!local_user_name.empty()) {
    if (!IsUserNameSafeForUnitName(local_user_name)) {
      return base::unexpected(base::StrCat(
          {"The username '", local_user_name, "' is not supported."}));
    }
    return base::ok();
  }
  if (target_host_type.requires_local_user()) {
    return base::unexpected(
        base::StrCat({"A local user is required, since the ",
                      target_host_type.name(), " host type needs one."}));
  }
  if (current_host_type && !current_host_type->is_multi_process()) {
    return base::unexpected(base::StrCat(
        {"A local user is required, since the configured ",
         current_host_type->name(), " host belongs to a local user."}));
  }
  return base::ok();
}

}  // namespace remoting
