// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdm_remote_user_desktop_session_creator.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_DisplayManager.h"
#include "remoting/host/linux/gdm_dbus_constants.h"

namespace remoting {

namespace {

// Timeout for the `CreateUserDisplay` call. GDM checks polkit synchronously
// before replying. This is longer than callers typically wait for the session
// to show up, so that their deadline is the effective one, while not leaving
// the call pending forever if GDM never replies.
constexpr base::TimeDelta kCreateUserDisplayTimeout = base::Seconds(40);

// Substrings of standard D-Bus errors returned when GDM isn't running or is
// too old to support `CreateUserDisplay`.
constexpr std::string_view kDBusUnknownMethodError =
    "org.freedesktop.DBus.Error.UnknownMethod";
constexpr std::string_view kDBusServiceUnknownError =
    "org.freedesktop.DBus.Error.ServiceUnknown";

}  // namespace

GdmRemoteUserDesktopSessionCreator::GdmRemoteUserDesktopSessionCreator(
    GDBusConnectionRef connection)
    : connection_(std::move(connection)) {
  DCHECK(connection_.is_initialized());
}

GdmRemoteUserDesktopSessionCreator::~GdmRemoteUserDesktopSessionCreator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GdmRemoteUserDesktopSessionCreator::CreateSession(
    const std::string& username,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HOST_LOG << "Asking GDM to create a remote session for user " << username
           << ".";
  connection_
      .Call<org_gnome_DisplayManager_RemoteDisplayFactory::CreateUserDisplay>(
          kGdmBusName, kGdmRemoteDisplayFactoryPath, std::tuple(username),
          base::BindOnce(
              &GdmRemoteUserDesktopSessionCreator::OnCreateUserDisplayResult,
              weak_ptr_factory_.GetWeakPtr(), username, std::move(callback)),
          G_DBUS_CALL_FLAGS_NONE,
          base::checked_cast<int>(kCreateUserDisplayTimeout.InMilliseconds()));
}

// static
bool GdmRemoteUserDesktopSessionCreator::IsNotSupportedError(
    std::string_view error) {
  return error.contains(kDBusUnknownMethodError) ||
         error.contains(kDBusServiceUnknownError);
}

void GdmRemoteUserDesktopSessionCreator::OnCreateUserDisplayResult(
    std::string username,
    Callback callback,
    base::expected<std::tuple<>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.has_value()) {
    HOST_LOG << "GDM accepted the request to create a remote session for user "
             << username << ". If no session appears, check that GDM's "
             << "`gdm-direct-login` PAM service (or `gdm-autologin` on older "
             << "GDM) allows the user to log in.";
    std::move(callback).Run(base::ok());
    return;
  }

  if (IsNotSupportedError(result.error().ToString())) {
    LOG(ERROR) << "Creating remote user sessions requires GDM 50 or newer "
               << "to be running.";
  }
  std::move(callback).Run(base::unexpected(std::move(result).error()));
}

}  // namespace remoting
