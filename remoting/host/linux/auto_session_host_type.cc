// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/auto_session_host_type.h"

#include <string_view>

#include "base/no_destructor.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

AutoSessionHostType::~AutoSessionHostType() = default;

std::string_view AutoSessionHostType::name() const {
  return "auto-session";
}

std::string_view AutoSessionHostType::description() const {
  return "Multi-process host. GDM creates a GNOME session for the user on "
         "connect if none exists. Requires GDM 50 or later.";
}

bool AutoSessionHostType::requires_local_user() const {
  return true;
}

bool AutoSessionHostType::ValidateStartHostHookContext(
    const HostTypeContext& context) const {
  return HostType::ValidateStartHostHookContext(context) &&
         CheckUserNameForSessionRouting(context);
}

bool AutoSessionHostType::SetUpState(const HostTypeContext& context) const {
  // RunStartHostHook() has validated the local user.
  return MultiProcessHostType::SetUpState(context) &&
         WriteSessionRoutingConfig(context,
                                   /*create_remote_user_sessions=*/true);
}

const HostType& GetAutoSessionHostType() {
  static const base::NoDestructor<AutoSessionHostType> instance;
  return *instance;
}

}  // namespace remoting
