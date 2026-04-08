// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/screen_saver_inhibitor.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_ScreenSaver.h"

namespace remoting {

namespace {

constexpr char kScreenSaverBusName[] = "org.freedesktop.ScreenSaver";
constexpr char kScreenSaverObjectPath[] = "/org/freedesktop/ScreenSaver";

}  // namespace

ScreenSaverInhibitor::ScreenSaverInhibitor(GDBusConnectionRef connection,
                                           std::string_view reason_for_inhibit)
    : connection_(connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Call<org_freedesktop_ScreenSaver::Inhibit>(
      kScreenSaverBusName, kScreenSaverObjectPath,
      std::tuple(/*application_name=*/"Chrome Remote Desktop",
                 reason_for_inhibit),
      base::BindOnce(&ScreenSaverInhibitor::OnInhibited,
                     weak_ptr_factory_.GetWeakPtr()));
}

ScreenSaverInhibitor::~ScreenSaverInhibitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!cookie_.has_value()) {
    return;
  }
  // We bind `connection_` to the callback to make sure the call goes through
  // after the destruction.
  connection_.Call<org_freedesktop_ScreenSaver::UnInhibit>(
      kScreenSaverBusName, kScreenSaverObjectPath, std::tuple(*cookie_),
      base::BindOnce(
          [](GDBusConnectionRef connection,
             base::expected<std::tuple<>, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to uninhibit screen saver: "
                         << result.error().ToString();
            }
          },
          connection_),
      G_DBUS_CALL_FLAGS_NONE, /*timeout_msec=*/10000);
}

void ScreenSaverInhibitor::OnInhibited(
    base::expected<std::tuple<uint32_t>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    auto& [cookie] = result.value();
    cookie_ = cookie;
    HOST_LOG << "Screen saver inhibited. Cookie: " << cookie;
  } else {
    LOG(ERROR) << "Failed to inhibit screen saver: "
               << result.error().ToString();
  }
}

}  // namespace remoting
