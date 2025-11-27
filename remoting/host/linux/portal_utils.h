// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_UTILS_H_
#define REMOTING_HOST_LINUX_PORTAL_UTILS_H_

#include <string_view>

#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

inline constexpr char kPortalBusName[] = "org.freedesktop.portal.Desktop";

inline constexpr gvariant::ObjectPathCStr kPortalObjectPath =
    "/org/freedesktop/portal/desktop";

// Generate a random token that is in the form of `${prefix}_${random_number}`.
std::string GeneratePortalToken(std::string_view prefix);

// Returns a handle object path in the form of:
//   ${kPortalObjectPath}/${object_type}/${sender}/${token}
// Or error if failed to get the object path.
//
// `object_type` should be either "request" or "session".
base::expected<gvariant::ObjectPath, Loggable> GetPortalHandle(
    GDBusConnectionRef connection,
    std::string_view object_type,
    std::string_view token);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_UTILS_H_
