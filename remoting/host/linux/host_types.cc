// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "remoting/host/linux/host_type_environment.h"

namespace remoting {
namespace {

constexpr char kSingleProcessUnitPrefix[] = "chrome-remote-desktop@";

std::string GetSingleProcessUnit(std::string_view local_user_name) {
  return base::StrCat({kSingleProcessUnitPrefix, local_user_name, ".service"});
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
                  " Unsaved work in its desktop session will be lost.");
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
  bool IsCurrent(const HostTypeEnvironment& env,
                 std::string_view local_user_name) const override;
  bool HasPerUserUnits(const HostTypeEnvironment& env) const override;

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
  bool IsCurrent(const HostTypeEnvironment& env,
                 std::string_view local_user_name) const override;

 protected:
  bool StartHostUnit(const HostTypeContext& context) const override;
  bool StopHostUnit(const HostTypeContext& context) const override;

  // Returns whether the state of the system (e.g. the session routing config
  // file) matches this host type, assuming that a multi-process host is
  // running and that no multi-process host type earlier in the detection
  // order matched. Performs blocking operations.
  virtual bool MatchesState(const HostTypeEnvironment& env,
                            std::string_view local_user_name) const = 0;

  // HostType implementation. Stops and disables the single-process host unit
  // of the local user if it is enabled or active, e.g. after the
  // single-process host was stopped without its stop hook, since both host
  // units can't be used at the same time. Subclasses that override
  // SetUpState() must call this first.
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
  bool MatchesState(const HostTypeEnvironment& env,
                    std::string_view local_user_name) const override;
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

bool SingleProcessHostType::IsCurrent(const HostTypeEnvironment& env,
                                      std::string_view local_user_name) const {
  return !local_user_name.empty() &&
         env.IsUnitActive(GetSingleProcessUnit(local_user_name));
}

bool SingleProcessHostType::HasPerUserUnits(
    const HostTypeEnvironment& env) const {
  // Matches the host unit of any user.
  return env.HasEnabledOrActiveUnits(
      base::StrCat({kSingleProcessUnitPrefix, "*.service"}));
}

bool SingleProcessHostType::StartHostUnit(
    const HostTypeContext& context) const {
  if (context.local_user_name.empty()) {
    std::cerr << "Cannot start the single-process host because the local "
                 "user is unknown.\n";
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

bool MultiProcessHostType::IsCurrent(const HostTypeEnvironment& env,
                                     std::string_view local_user_name) const {
  return env.IsUnitActive(kMultiProcessHostUnit) &&
         MatchesState(env, local_user_name);
}

bool MultiProcessHostType::StartHostUnit(const HostTypeContext& context) const {
  return StartUnit(*context.env, kMultiProcessHostUnit);
}

bool MultiProcessHostType::StopHostUnit(const HostTypeContext& context) const {
  return StopMultiProcessUnit(*context.env);
}

bool MultiProcessHostType::SetUpState(const HostTypeContext& context) const {
  return StopSingleProcessUnit(context);
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

bool GdmManagedHostType::MatchesState(const HostTypeEnvironment& env,
                                      std::string_view local_user_name) const {
  // Last in the detection order, so it matches any running multi-process host
  // that no other host type matched.
  // TODO(crbug.com/493980875): Only match if the session routing config file
  // doesn't exist once a host type that matches it exists.
  return true;
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
const HostType* HostType::DetectCurrent(const HostTypeEnvironment& env,
                                        std::string_view local_user_name) {
  // A host type is only checked if no host type before it matched.
  static const HostType* const kHostTypesInDetectionOrder[] = {
      &GetSingleProcessHostType(),
      &GetGdmManagedHostType(),
  };
  for (const HostType* type : kHostTypesInDetectionOrder) {
    if (type->IsCurrent(env, local_user_name)) {
      return type;
    }
  }
  return nullptr;
}

bool HostType::HasPerUserUnits(const HostTypeEnvironment& env) const {
  return false;
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
  return SetUpState(context) && StartHostUnit(context);
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
    const HostTypeEnvironment& env,
    const HostType& target_host_type,
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
  for (const auto& [name, host_type] : HostType::GetHostTypes()) {
    if (host_type->HasPerUserUnits(env)) {
      return base::unexpected(
          "A local user is required, since a per-user Chrome Remote Desktop "
          "unit is enabled or active.");
    }
  }
  return base::ok();
}

}  // namespace remoting
