// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.systemd1.Service.xml and
// manually edited to remove unneeded parts.

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_SERVICE_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_SERVICE_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_systemd1_Service {

// property
struct ExecStart {
  static constexpr char kInterfaceName[] = "org.freedesktop.systemd1.Service";
  static constexpr char kPropertyName[] = "ExecStart";
  static constexpr gvariant::Type kType{"a(sasbttttuii)"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_freedesktop_systemd1_Service

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_SERVICE_H_
