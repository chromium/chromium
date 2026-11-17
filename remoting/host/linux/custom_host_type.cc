// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/custom_host_type.h"

#include <string_view>

#include "base/no_destructor.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

CustomHostType::~CustomHostType() = default;

std::string_view CustomHostType::name() const {
  return "custom";
}

std::string_view CustomHostType::description() const {
  return "Multi-process host that connects to a graphical session that you "
         "start yourself, or to a local graphical session if curtain mode is "
         "disabled.";
}

bool CustomHostType::requires_local_user() const {
  return true;
}

bool CustomHostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  return HostType::ValidateStartHostHookContext(context) &&
         CheckUserNameForSessionRouting(context);
}

bool CustomHostType::SetUpState(const HostTypeContext& context) const {
  // RunStartHostHook() has validated the local user.
  return MultiProcessHostType::SetUpState(context) &&
         WriteSessionRoutingConfig(context,
                                   /*create_remote_user_sessions=*/false);
}

const HostType& GetCustomHostType() {
  static const base::NoDestructor<CustomHostType> instance;
  return *instance;
}

}  // namespace remoting
