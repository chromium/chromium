// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_action_executor.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_ScreenSaver.h"
#include "remoting/proto/action.pb.h"

namespace remoting {

GnomeActionExecutor::GnomeActionExecutor(GDBusConnectionRef connection)
    : connection_(std::move(connection)) {}

GnomeActionExecutor::~GnomeActionExecutor() = default;

void GnomeActionExecutor::ExecuteAction(
    const protocol::ActionRequest& request) {
  switch (request.action()) {
    case protocol::ActionRequest::LOCK_WORKSTATION:
      connection_.Call<org_gnome_ScreenSaver::Lock>(
          "org.gnome.ScreenSaver", "/org/gnome/ScreenSaver", std::tuple(),
          base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
            if (!result.has_value()) {
              LOG(WARNING) << "Failed to lock screen: " << result.error();
            }
          }));
      break;
    default:
      break;
  }
}

}  // namespace remoting
