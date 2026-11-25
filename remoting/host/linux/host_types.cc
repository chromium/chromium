// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
#include "remoting/base/secure_file_reader.h"
#include "remoting/host/linux/auto_session_host_type.h"
#include "remoting/host/linux/custom_host_type.h"
#include "remoting/host/linux/gdm_managed_host_type.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/session_routing_config.h"
#include "remoting/host/linux/unit_session_host_type.h"

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

bool FileOrSymlinkExists(const base::FilePath& path) {
  return base::PathExists(path) || base::IsLink(path);
}

// Returns the error message for an invalid host type record at `path`.
// `reason` describes what is wrong with it.
std::string InvalidHostTypeRecordError(const base::FilePath& path,
                                       std::string_view reason) {
  return base::StrCat({"The host type record '", path.value(), "' is invalid. ",
                       reason, "\nFix or delete the file, then try again."});
}

// Stops and disables `unit` if it is enabled or active. Prints `warning` (may
// be empty) when stopping an active unit. Returns false on failure.
bool StopUnit(HostTypeEnvironment& env,
              std::string_view unit,
              std::string_view warning) {
  std::string unit_str(unit);
  bool active = env.IsUnitActive(unit_str);
  if (!active && !env.IsUnitEnabled(unit_str)) {
    return true;
  }
  if (active) {
    std::cout << "Stopping " << unit << "." << warning << "\n";
  }
  if (!env.DisableAndStopUnit(unit_str)) {
    std::cerr << "Failed to stop " << unit << ".\n";
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

// Stops and disables the multi-process host unit if it is enabled or active.
// Returns false on failure.
bool StopMultiProcessUnit(HostTypeEnvironment& env) {
  return StopUnit(env, kMultiProcessHostUnit, /*warning=*/"");
}

// Base class of the single-process host type, whose host unit is
// `chrome-remote-desktop@<user>.service`. It has no other state. Its
// SetUpState() stops and disables the multi-process host unit and the session
// units of all users if they are enabled or active, e.g. after the
// multi-process host was stopped without its stop hook, since they can't be
// used at the same time as the single-process host.
class SingleProcessHostType : public HostType {
 public:
  ~SingleProcessHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool is_multi_process() const override;
  bool requires_local_user() const override;

 protected:
  bool StartHostUnit(const HostTypeContext& context) const override;
  bool StopHostUnit(const HostTypeContext& context) const override;
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

bool SingleProcessHostType::StartHostUnit(
    const HostTypeContext& context) const {
  // Checked by ValidateStartHostHookContext().
  CHECK(!context.local_user_name.empty());
  return StartUnit(*context.env, GetSingleProcessUnit(context.local_user_name));
}

bool SingleProcessHostType::StopHostUnit(const HostTypeContext& context) const {
  return StopSingleProcessUnit(context);
}

bool SingleProcessHostType::SetUpState(const HostTypeContext& context) const {
  return StopMultiProcessUnit(*context.env) &&
         UnitSessionHostType::StopAllSessionUnits(*context.env);
}

const SingleProcessHostType& GetSingleProcessHostType() {
  static const base::NoDestructor<SingleProcessHostType> instance;
  return *instance;
}

const HostType& GetX11HostType() {
  static const base::NoDestructor<UnitSessionHostType> instance(
      "x11",
      "Multi-process host. Chrome Remote Desktop starts an X11 desktop "
      "session for the user, and keeps it running. Requires an installed "
      "desktop environment that supports X11 (not supported by newer "
      "versions of GNOME).",
      "chrome-remote-desktop-x11-session@");
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
          {GetCustomHostType().name(), &GetCustomHostType()},
          {GetX11HostType().name(), &GetX11HostType()},
          {GetAutoSessionHostType().name(), &GetAutoSessionHostType()},
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
    const HostTypeEnvironment& env) {
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
    return base::ok(GetFallbackConfigured(env));
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
    const HostTypeEnvironment& env) {
  // gdm-managed requires that there is no session routing config file, while
  // custom works with any of them.
  const HostType& multi_process_host_type =
      FileOrSymlinkExists(env.GetSessionRoutingConfigPath())
          ? GetCustomHostType()
          : GetGdmManagedHostType();
  bool has_multi_process_config = env.MultiProcessConfigExists();

  // 1. The multi-process host config exists and the multi-process host unit
  //    is enabled.
  if (has_multi_process_config && env.IsUnitEnabled(kMultiProcessHostUnit)) {
    return &multi_process_host_type;
  }

  // 2. Any single-process host unit is enabled or active. It stays enabled
  //    when the single-process host is stopped or deleted, and takes
  //    precedence over a multi-process host config that is left over from
  //    `migrate-host --keep-config`.
  std::optional<std::vector<std::string>> single_process_units =
      env.ListEnabledOrActiveUnits(GetSingleProcessUnit("*"));
  if (single_process_units.has_value() && !single_process_units->empty()) {
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
  if (!requires_local_user()) {
    return true;
  }
  if (context.local_user_name.empty()) {
    std::cerr << "The " << name() << " host type requires a local user.\n";
    return false;
  }
  if (!IsUserNameSafeForUnitName(context.local_user_name)) {
    std::cerr << "The " << name() << " host type doesn't support the username '"
              << context.local_user_name << "'.\n";
    return false;
  }
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

// static
bool HostType::StartUnit(HostTypeEnvironment& env, std::string_view unit) {
  std::cout << "Starting " << unit << ".\n";
  if (!env.EnableAndStartUnit(std::string(unit))) {
    std::cerr << "Failed to start " << unit << ".\n";
    return false;
  }
  return true;
}

// static
bool HostType::StopUnitsMatching(HostTypeEnvironment& env,
                                 std::string_view pattern,
                                 std::string_view active_unit_warning,
                                 std::string_view except_unit) {
  std::optional<std::vector<std::string>> units =
      env.ListEnabledOrActiveUnits(pattern);
  if (!units.has_value()) {
    std::cerr << "Failed to list the units matching " << pattern << ".\n";
    return false;
  }
  for (const std::string& unit : *units) {
    if (unit != except_unit && !StopUnit(env, unit, active_unit_warning)) {
      return false;
    }
  }
  return true;
}

// MultiProcessHostType

MultiProcessHostType::MultiProcessHostType() = default;

MultiProcessHostType::~MultiProcessHostType() = default;

// static
bool MultiProcessHostType::StopAllSingleProcessUnits(HostTypeEnvironment& env) {
  return StopUnitsMatching(env, GetSingleProcessUnit("*"),
                           kSingleProcessUnitStopWarning);
}

// static
bool MultiProcessHostType::DeleteSessionRoutingConfig(
    HostTypeEnvironment& env) {
  base::FilePath path = env.GetSessionRoutingConfigPath();
  bool existed = FileOrSymlinkExists(path);
  if (!base::DeleteFile(path)) {
    std::cerr << "Failed to delete " << path << ".\n";
    return false;
  }
  if (existed) {
    std::cout << "Deleted the session routing config file " << path << ".\n";
  }
  return true;
}

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
  return StopAllSingleProcessUnits(*context.env) &&
         UnitSessionHostType::StopAllSessionUnits(*context.env);
}

bool MultiProcessHostType::CleanUpState(const HostTypeContext& context) const {
  return DeleteSessionRoutingConfig(*context.env);
}

// static
bool MultiProcessHostType::CheckUserNameForSessionRouting(
    const HostTypeContext& context) {
  if (!context.is_corp_host &&
      !SessionRoutingConfig::IsValidUsername(context.local_user_name)) {
    std::cerr << "Username '" << context.local_user_name
              << "' is not supported for session routing on this host. Only "
                 "letters, digits, '_', '.' and '-' are allowed, and the "
                 "name must not start with '-'.\n";
    return false;
  }
  return true;
}

// static
bool MultiProcessHostType::WriteSessionRoutingConfig(
    const HostTypeContext& context,
    bool create_remote_user_sessions) {
  // For corp hosts, omit `sessionUser` so the session user is derived from
  // the client ID.
  std::string session_user =
      context.is_corp_host ? std::string() : context.local_user_name;
  SessionRoutingConfig config(std::move(session_user),
                              create_remote_user_sessions,
                              context.is_corp_host);
  auto result = config.WriteToFile(context.env->GetSessionRoutingConfigPath());
  if (!result.has_value()) {
    std::cerr << "Failed to write the session routing config file: "
              << result.error() << "\n";
    return false;
  }
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
