// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/unit_session_host_type.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {
namespace {

// Printed when stopping an active session unit.
constexpr char kSessionUnitStopWarning[] =
    " Unsaved work in the session will be lost.";

base::flat_set<const UnitSessionHostType*>& GetRegisteredInstances() {
  static base::NoDestructor<base::flat_set<const UnitSessionHostType*>>
      instances;
  return *instances;
}

}  // namespace

UnitSessionHostType::UnitSessionHostType(std::string_view name,
                                         std::string_view description,
                                         std::string_view unit_prefix)
    : name_(name), description_(description), unit_prefix_(unit_prefix) {
  GetRegisteredInstances().insert(this);
}

UnitSessionHostType::~UnitSessionHostType() {
  GetRegisteredInstances().erase(this);
}

std::string_view UnitSessionHostType::name() const {
  return name_;
}

std::string_view UnitSessionHostType::description() const {
  return description_;
}

bool UnitSessionHostType::requires_local_user() const {
  return true;
}

bool UnitSessionHostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  return HostType::ValidateStartHostHookContext(context) &&
         CheckUserNameForSessionRouting(context);
}

std::string UnitSessionHostType::GetSessionUnit(
    std::string_view local_user_name) const {
  CHECK(!local_user_name.empty());
  return base::StrCat({unit_prefix_, local_user_name, ".service"});
}

// static
bool UnitSessionHostType::StopAllSessionUnits(HostTypeEnvironment& env,
                                              std::string_view except_unit) {
  // Ensure built-in UnitSessionHostType singletons in GetHostTypes() are
  // constructed before iterating.
  HostType::GetHostTypes();
  for (const UnitSessionHostType* type : GetRegisteredInstances()) {
    if (!StopUnitsMatching(env, type->GetSessionUnit("*"),
                           kSessionUnitStopWarning, except_unit)) {
      return false;
    }
  }
  return true;
}

bool UnitSessionHostType::SetUpState(const HostTypeContext& context) const {
  // RunStartHostHook() has validated the local user.
  std::string session_unit = GetSessionUnit(context.local_user_name);
  if (!StopAllSingleProcessUnits(*context.env) ||
      !StopAllSessionUnits(*context.env, /*except_unit=*/session_unit) ||
      !WriteSessionRoutingConfig(context,
                                 /*create_remote_user_sessions=*/false)) {
    return false;
  }
  return StartUnit(*context.env, session_unit);
}

bool UnitSessionHostType::CleanUpState(const HostTypeContext& context) const {
  // The session routing config file is kept if a session unit fails to stop.
  // All users' session units are stopped, so that the local user isn't
  // needed.
  return StopUnitsMatching(*context.env, GetSessionUnit("*"),
                           kSessionUnitStopWarning) &&
         MultiProcessHostType::CleanUpState(context);
}

}  // namespace remoting
