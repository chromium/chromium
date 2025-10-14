// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_headless_detector.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_systemd1_Manager.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_systemd1_Service.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_systemd1_Unit.h"

namespace remoting {

namespace {

constexpr char kSystemdBusName[] = "org.freedesktop.systemd1";

}  // namespace

GnomeHeadlessDetector::GnomeHeadlessDetector() = default;
GnomeHeadlessDetector::~GnomeHeadlessDetector() = default;

void GnomeHeadlessDetector::Start(GDBusConnectionRef connection,
                                  Callback callback) {
  dbus_connection_ = std::move(connection);
  callback_ = std::move(callback);

  dbus_connection_.Call<org_freedesktop_systemd1_Manager::GetUnit>(
      kSystemdBusName, "/org/freedesktop/systemd1",
      std::tuple("org.gnome.Shell@wayland.service"),
      base::BindOnce(&GnomeHeadlessDetector::OnGetUnitReply,
                     weak_factory_.GetWeakPtr()));
}

void GnomeHeadlessDetector::OnGetUnitReply(
    base::expected<std::tuple<gvariant::ObjectPath>, Loggable> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "GetUnit failed: " << result.error();
    std::move(callback_).Run(false);
    return;
  }

  systemd_unit_path_ = std::get<0>(result.value());

  dbus_connection_.GetProperty<org_freedesktop_systemd1_Unit::ActiveState>(
      kSystemdBusName, systemd_unit_path_,
      base::BindOnce(&GnomeHeadlessDetector::OnActiveStateReply,
                     weak_factory_.GetWeakPtr()));
}

void GnomeHeadlessDetector::OnActiveStateReply(
    base::expected<std::string, Loggable> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to get ActiveState: " << result.error();
    std::move(callback_).Run(false);
    return;
  }

  if (result.value() != "active") {
    LOG(ERROR) << "GNOME Shell service is not running, ActiveState returned: "
               << result.value();
    std::move(callback_).Run(false);
    return;
  }

  dbus_connection_.GetProperty<org_freedesktop_systemd1_Service::ExecStart>(
      kSystemdBusName, systemd_unit_path_,
      base::BindOnce(&GnomeHeadlessDetector::OnExecStartReply,
                     weak_factory_.GetWeakPtr()));
}

void GnomeHeadlessDetector::OnExecStartReply(
    base::expected<std::vector<GVariantRef<"(sasbttttuii)">>, Loggable>
        result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to get ExecStart: " << result.error();
    std::move(callback_).Run(false);
    return;
  }

  if (result.value().size() != 1) {
    LOG(ERROR) << "Expected ExecStart to have 1 element, got "
               << result.value().size();
    std::move(callback_).Run(false);
    return;
  }

  const auto& exec_start = result.value()[0];

  auto exe = exec_start.get<0>().Into<std::string>();
  constexpr char kGnomeShellPath[] = "/usr/bin/gnome-shell";
  if (exe != kGnomeShellPath) {
    LOG(ERROR) << "Expected " << kGnomeShellPath << ", but got " << exe;
    std::move(callback_).Run(false);
    return;
  }

  auto args = exec_start.get<1>().Into<std::vector<std::string>>();
  constexpr char kHeadlessArg[] = "--headless";
  if (!base::Contains(args, kHeadlessArg)) {
    HOST_LOG << "Did not find '" << kHeadlessArg << "' in gnome-shell args";
    std::move(callback_).Run(false);
    return;
  }

  HOST_LOG << "GNOME Wayland session is headless.";
  std::move(callback_).Run(true);
}

}  // namespace remoting
