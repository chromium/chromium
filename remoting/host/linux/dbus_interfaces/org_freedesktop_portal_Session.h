// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.portal.Session.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SESSION_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SESSION_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_portal_Session {

// method
struct Close {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Session";
  static constexpr char kMethodName[] = "Close";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct version {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Session";
  static constexpr char kPropertyName[] = "version";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct Closed {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Session";
  static constexpr char kSignalName[] = "Closed";
  static constexpr gvariant::Type kType{
      "("
      "a{sv}"  // details
      ")"};
};

}  // namespace remoting::org_freedesktop_portal_Session

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SESSION_H_
