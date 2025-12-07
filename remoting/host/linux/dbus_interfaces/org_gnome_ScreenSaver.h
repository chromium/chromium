// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.gnome.ScreenSaver.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_SCREENSAVER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_SCREENSAVER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_gnome_ScreenSaver {

// method
struct Lock {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kMethodName[] = "Lock";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct GetActive {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kMethodName[] = "GetActive";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "b"  // active
      ")"};
};

// method
struct SetActive {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kMethodName[] = "SetActive";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // value
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct GetActiveTime {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kMethodName[] = "GetActiveTime";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"  // value
      ")"};
};

// signal
struct ActiveChanged {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kSignalName[] = "ActiveChanged";
  static constexpr gvariant::Type kType{
      "("
      "b"  // new_value
      ")"};
};

// signal
struct WakeUpScreen {
  static constexpr char kInterfaceName[] = "org.gnome.ScreenSaver";
  static constexpr char kSignalName[] = "WakeUpScreen";
  static constexpr gvariant::Type kType{"()"};
};

}  // namespace remoting::org_gnome_ScreenSaver

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_SCREENSAVER_H_
