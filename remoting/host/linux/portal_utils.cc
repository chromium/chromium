// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_utils.h"

#include <gio/gio.h>

#include <string>

#include "base/logging.h"

namespace remoting {

std::string GeneratePortalToken(std::string_view prefix) {
  return base::StringPrintf("%s_%d", prefix, g_random_int_range(0, G_MAXINT));
}

base::expected<gvariant::ObjectPath, Loggable> GetPortalHandle(
    GDBusConnectionRef connection,
    std::string_view object_type,
    std::string_view token) {
  // `sender` is the callers unique name, with the initial ':' removed and all
  // '.' replaced by '_'.
  const char* unique_name = g_dbus_connection_get_unique_name(connection.raw());
  std::string sender = unique_name ? unique_name : std::string{};
  if (sender.starts_with(':')) {
    sender = sender.substr(1);
  }
  std::replace(sender.begin(), sender.end(), '.', '_');
  return gvariant::ObjectPath::TryFrom(base::StringPrintf(
      "%s/%s/%s/%s", kPortalObjectPath.c_str(), object_type, sender, token));
}

}  // namespace remoting
