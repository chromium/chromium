// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.portal.Request.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REQUEST_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REQUEST_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_portal_Request {

// method
struct Close {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Request";
  static constexpr char kMethodName[] = "Close";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// signal
struct Response {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Request";
  static constexpr char kSignalName[] = "Response";
  static constexpr gvariant::Type kType{
      "("
      "u"      // response
      "a{sv}"  // results
      ")"};
};

}  // namespace remoting::org_freedesktop_portal_Request

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REQUEST_H_
