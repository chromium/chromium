// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_SCOPED_PORTAL_REQUEST_H_
#define REMOTING_HOST_LINUX_SCOPED_PORTAL_REQUEST_H_

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// RAII helper class for a portal request. Destroying a ScopedPortalRequest
// object closes the request.
//
// See:
// https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html
//
// Usage:
//
// ```
// auto request = std::make_unique<ScopedPortalRequest>(
//     connection, std::move(response_callback));
// auto portal_dbus_call_options =
//     GVariantDictBuilder().Add("handle_token", request->token()).Build();
// // Make the D-Bus call...
// // In `response_callback`, reset `request`.
// // In the D-Bus call's callback, reset `request` if and only if the call
// // fails.
// ```
class ScopedPortalRequest {
 public:
  using ResponseCallback = base::OnceCallback<void(
      base::expected<gvariant::GVariantRef<"a{sv}">, Loggable>)>;

  ScopedPortalRequest(GDBusConnectionRef connection, ResponseCallback callback);
  ~ScopedPortalRequest();

  // A token that can be used as the `handle_token` option when making a portal
  // D-Bus call.
  const std::string& token() const { return token_; }

  // Closes the request. The callback will not be called after this method is
  // called.
  void Close();

 private:
  void OnResponse(std::string sender,
                  gvariant::ObjectPath object_path,
                  std::string interface_name,
                  std::string signal_name,
                  gvariant::GVariantRef<"(ua{sv})"> arguments);

  GDBusConnectionRef connection_;
  ResponseCallback callback_;
  std::string token_;
  gvariant::ObjectPath request_handle_;
  std::unique_ptr<GDBusConnectionRef::SignalSubscription> subscription_;

  base::WeakPtrFactory<ScopedPortalRequest> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_SCOPED_PORTAL_REQUEST_H_
