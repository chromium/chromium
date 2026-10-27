// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.host.portal.Registry.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_HOST_PORTAL_REGISTRY_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_HOST_PORTAL_REGISTRY_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_host_portal_Registry {

// method
struct Register {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.host.portal.Registry";
  static constexpr char kMethodName[] = "Register";
  static constexpr gvariant::Type kInType{
      "("
      "s"      // app_id
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct version {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.host.portal.Registry";
  static constexpr char kPropertyName[] = "version";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_freedesktop_host_portal_Registry

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_HOST_PORTAL_REGISTRY_H_
