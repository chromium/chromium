// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/scoped_portal_session.h"

#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_Session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/portal_utils.h"

namespace remoting {

ScopedPortalSession::ScopedPortalSession(GDBusConnectionRef connection,
                                         ClosedCallback callback)
    : connection_(connection), callback_(std::move(callback)) {
  token_ = GeneratePortalToken("remoting_session");
  auto session_handle_expected =
      GetPortalHandle(connection_, "session", token_);
  if (!session_handle_expected.has_value()) {
    LOG(ERROR) << "Failed to get portal session handle: "
               << session_handle_expected.error();
    std::move(callback_).Run(
        gvariant::GVariantFrom(gvariant::EmptyArrayOf<"{sv}">()));
    return;
  }
  session_handle_ = *session_handle_expected;
  subscription_ =
      connection_.SignalSubscribe<org_freedesktop_portal_Session::Closed>(
          kPortalBusName, session_handle_,
          base::BindRepeating(&ScopedPortalSession::OnClosed,
                              weak_ptr_factory_.GetWeakPtr()));
}

ScopedPortalSession::~ScopedPortalSession() {
  Close();
}

void ScopedPortalSession::Close() {
  if (!connection_.is_initialized()) {
    return;
  }

  connection_.Call<org_freedesktop_portal_Session::Close>(
      kPortalBusName, session_handle_, std::tuple<>(),
      base::BindOnce([](base::expected<std::tuple<>, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << "Failed to close portal session: " << result.error();
        }
      }));

  connection_ = {};
  subscription_.reset();
  callback_.Reset();
}

void ScopedPortalSession::OnClosed(std::string sender,
                                   gvariant::ObjectPath object_path,
                                   std::string interface_name,
                                   std::string signal_name,
                                   gvariant::GVariantRef<"(a{sv})"> arguments) {
  // Prevent Close() from being called.
  subscription_.reset();
  connection_ = {};

  if (callback_) {
    gvariant::GVariantRef<"a{sv}"> details;
    arguments.Destructure(details);
    std::move(callback_).Run(details);
  }
}

}  // namespace remoting
