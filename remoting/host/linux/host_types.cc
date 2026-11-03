// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <iostream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "remoting/host/linux/host_type_environment.h"

namespace remoting {
namespace {

constexpr char kSingleProcessUnitPrefix[] = "chrome-remote-desktop@";
constexpr char kMultiProcessUnit[] = "chrome-remote-desktop.service";

bool IsMultiProcessServiceActive(const HostTypeEnvironment& env) {
  return env.IsUnitActive(kMultiProcessUnit);
}

class SingleProcessHostType : public HostType {
 public:
  ~SingleProcessHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool is_multi_process() const override;
  bool requires_local_user() const override;
  bool IsCurrent(const HostTypeEnvironment& env,
                 std::string_view local_user_name) const override;
};

class GdmManagedHostType : public HostType {
 public:
  ~GdmManagedHostType() override;
  std::string_view name() const override;
  std::string_view description() const override;
  bool is_multi_process() const override;
  bool requires_local_user() const override;
  bool IsCurrent(const HostTypeEnvironment& env,
                 std::string_view local_user_name) const override;
  bool RunStartHostHook(const HostTypeContext& context) const override;
};

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
         env.IsUnitActive(base::StrCat(
             {kSingleProcessUnitPrefix, local_user_name, ".service"}));
}

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

bool GdmManagedHostType::is_multi_process() const {
  return true;
}

bool GdmManagedHostType::requires_local_user() const {
  return false;
}

bool GdmManagedHostType::IsCurrent(const HostTypeEnvironment& env,
                                   std::string_view local_user_name) const {
  // TODO(crbug.com/493980875): Also require the absence of the session routing
  // config file once other multi-process host types exist.
  return IsMultiProcessServiceActive(env);
}

bool GdmManagedHostType::RunStartHostHook(
    const HostTypeContext& context) const {
  // GDM only manages the sessions if the session routing config file doesn't
  // exist. The stop hook of the previous host type is responsible for removing
  // its file, so a file that exists here may be hand-written, and is not
  // removed. A dangling symlink also counts as existing.
  base::FilePath path = context.env->GetSessionRoutingConfigPath();
  if (base::PathExists(path) || base::IsLink(path)) {
    std::cerr << "Cannot set up the gdm-managed host type because " << path
              << " exists. It may have been left behind by a different host "
                 "type. Move or delete it, and try again.\n";
    return false;
  }
  return true;
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

bool HostType::RunStartHostHook(const HostTypeContext& context) const {
  return true;
}

bool HostType::RunStopHostHook(const HostTypeContext& context) const {
  return true;
}

}  // namespace remoting
