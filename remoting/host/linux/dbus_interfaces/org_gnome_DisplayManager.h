// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.gnome.DisplayManager.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_DISPLAYMANAGER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_DISPLAYMANAGER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_gnome_DisplayManager_RemoteDisplayFactory {

// method
struct CreateRemoteDisplay {
  static constexpr char kInterfaceName[] =
      "org.gnome.DisplayManager.RemoteDisplayFactory";
  static constexpr char kMethodName[] = "CreateRemoteDisplay";
  static constexpr gvariant::Type kInType{
      "("
      "o"  // remote_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

}  // namespace remoting::org_gnome_DisplayManager_RemoteDisplayFactory

namespace remoting::org_gnome_DisplayManager_RemoteDisplay {

// property
struct RemoteId {
  static constexpr char kInterfaceName[] =
      "org.gnome.DisplayManager.RemoteDisplay";
  static constexpr char kPropertyName[] = "RemoteId";
  static constexpr gvariant::Type kType{"o"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct SessionId {
  static constexpr char kInterfaceName[] =
      "org.gnome.DisplayManager.RemoteDisplay";
  static constexpr char kPropertyName[] = "SessionId";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_gnome_DisplayManager_RemoteDisplay

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_DISPLAYMANAGER_H_
