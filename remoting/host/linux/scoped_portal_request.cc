// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/scoped_portal_request.h"

#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_portal_Request.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/portal_utils.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

ScopedPortalRequest::ScopedPortalRequest(GDBusConnectionRef connection,
                                         ResponseCallback callback)
    : connection_(connection), callback_(std::move(callback)) {
  token_ = GeneratePortalToken("remoting_request");
  auto request_handle_expected =
      GetPortalHandle(connection_, "request", token_);
  if (!request_handle_expected.has_value()) {
    std::move(callback_).Run(base::unexpected(request_handle_expected.error()));
    return;
  }
  request_handle_ = *request_handle_expected;
  subscription_ =
      connection_.SignalSubscribe<org_freedesktop_portal_Request::Response>(
          kPortalBusName, request_handle_,
          base::BindRepeating(&ScopedPortalRequest::OnResponse,
                              weak_ptr_factory_.GetWeakPtr()));
}

ScopedPortalRequest::~ScopedPortalRequest() {
  Close();
}

void ScopedPortalRequest::Close() {
  if (!connection_.is_initialized()) {
    return;
  }

  connection_.Call<org_freedesktop_portal_Request::Close>(
      kPortalBusName, request_handle_, std::tuple<>(),
      base::BindOnce(
          [](const std::string& request_handle,
             base::expected<std::tuple<>, Loggable> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to close portal request " << request_handle
                         << ": " << result.error();
            }
          },
          request_handle_.value()));

  connection_ = {};
  subscription_.reset();
  callback_.Reset();
}

void ScopedPortalRequest::OnResponse(
    std::string sender,
    gvariant::ObjectPath object_path,
    std::string interface_name,
    std::string signal_name,
    gvariant::GVariantRef<"(ua{sv})"> arguments) {
  // Prevent Close() from being called.
  subscription_.reset();
  connection_ = {};

  if (!callback_) {
    // Close() has been called.
    return;
  }

  uint32_t response_code;
  gvariant::GVariantRef<"a{sv}"> results;
  arguments.Destructure(response_code, results);

  if (response_code != 0) {
    webrtc::Scoped<char> print_result(
        g_variant_print(results.raw(), /*type_annotate=*/false));
    std::move(callback_).Run(base::unexpected(Loggable(
        FROM_HERE,
        base::StringPrintf("Portal request %s failed with code %u, result: %s",
                           request_handle_.value(), response_code,
                           print_result ? print_result.get() : "(null)"))));
    return;
  }

  std::move(callback_).Run(base::ok(results));
}

}  // namespace remoting
