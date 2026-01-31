// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.DBus.ObjectManager.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_DBus_ObjectManager {

// method
struct GetManagedObjects {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.ObjectManager";
  static constexpr char kMethodName[] = "GetManagedObjects";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a{oa{sa{sv}}}"  // object_paths_interfaces_and_properties
      ")"};
};

// signal
struct InterfacesAdded {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.ObjectManager";
  static constexpr char kSignalName[] = "InterfacesAdded";
  static constexpr gvariant::Type kType{
      "("
      "o"          // object_path
      "a{sa{sv}}"  // interfaces_and_properties
      ")"};
};

// signal
struct InterfacesRemoved {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.ObjectManager";
  static constexpr char kSignalName[] = "InterfacesRemoved";
  static constexpr gvariant::Type kType{
      "("
      "o"   // object_path
      "as"  // interfaces
      ")"};
};

}  // namespace remoting::org_freedesktop_DBus_ObjectManager

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_OBJECTMANAGER_H_
