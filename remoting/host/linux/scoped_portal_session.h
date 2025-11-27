// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SCOPED_PORTAL_SESSION_H_
#define REMOTING_HOST_LINUX_SCOPED_PORTAL_SESSION_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// RAII helper class for a portal session. Destroying a ScopedPortalSession
// object closes the session.
//
// See:
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Session.html
//
// Usage:
//
// ```
// auto session = std::make_unique<ScopedPortalSession>(
//     connection, std::move(closed_callback));
// auto portal_dbus_call_options =
//     GVariantDictBuilder()
//       .Add("session_handle_token", session->token())
//       .Build();
// // Make the D-Bus call...
// // In `closed_callback`, reset `session`.
// // In the D-Bus call's callback, reset `session` if and only if the call
// // fails.
// ```
class ScopedPortalSession {
 public:
  // Callback for when the session is closed externally.
  using ClosedCallback =
      base::OnceCallback<void(gvariant::GVariantRef<"a{sv}">)>;

  ScopedPortalSession(GDBusConnectionRef connection, ClosedCallback callback);
  ~ScopedPortalSession();

  ScopedPortalSession(const ScopedPortalSession&) = delete;
  ScopedPortalSession& operator=(const ScopedPortalSession&) = delete;

  // A token that can be used as the `session_handle_token` option when making a
  // portal D-Bus call.
  const std::string& token() const { return token_; }

  const gvariant::ObjectPath& session_handle() const { return session_handle_; }

  // Closes the session. This will not call the closed callback.
  void Close();

 private:
  void OnClosed(std::string sender,
                gvariant::ObjectPath object_path,
                std::string interface_name,
                std::string signal_name,
                gvariant::GVariantRef<"(a{sv})"> arguments);

  GDBusConnectionRef connection_;
  std::string token_;
  gvariant::ObjectPath session_handle_;
  ClosedCallback callback_;
  std::unique_ptr<GDBusConnectionRef::SignalSubscription> subscription_;

  base::WeakPtrFactory<ScopedPortalSession> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SCOPED_PORTAL_SESSION_H_
