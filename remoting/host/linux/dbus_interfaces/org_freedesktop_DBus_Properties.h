// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.DBus.Properties.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_PROPERTIES_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_PROPERTIES_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_DBus_Properties {

// method
struct Get {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.Properties";
  static constexpr char kMethodName[] = "Get";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // interface_name
      "s"  // property_name
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "v"  // value
      ")"};
};

// method
struct GetAll {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.Properties";
  static constexpr char kMethodName[] = "GetAll";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // interface_name
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "a{sv}"  // properties
      ")"};
};

// method
struct Set {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.Properties";
  static constexpr char kMethodName[] = "Set";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // interface_name
      "s"  // property_name
      "v"  // value
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// signal
struct PropertiesChanged {
  static constexpr char kInterfaceName[] = "org.freedesktop.DBus.Properties";
  static constexpr char kSignalName[] = "PropertiesChanged";
  static constexpr gvariant::Type kType{
      "("
      "s"      // interface_name
      "a{sv}"  // changed_properties
      "as"     // invalidated_properties
      ")"};
};

}  // namespace remoting::org_freedesktop_DBus_Properties

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_DBUS_PROPERTIES_H_
