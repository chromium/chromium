// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org_freedesktop_ScreenSaver.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SCREENSAVER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SCREENSAVER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_ScreenSaver {

// method
struct Lock {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "Lock";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SimulateUserActivity {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "SimulateUserActivity";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct GetActive {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "GetActive";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "b"  // arg_0
      ")"};
};

// method
struct GetActiveTime {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "GetActiveTime";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"  // seconds
      ")"};
};

// method
struct GetSessionIdleTime {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "GetSessionIdleTime";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"  // seconds
      ")"};
};

// method
struct SetActive {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "SetActive";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // e
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "b"  // arg_0
      ")"};
};

// method
struct Inhibit {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "Inhibit";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // application_name
      "s"  // reason_for_inhibit
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"  // cookie
      ")"};
};

// method
struct UnInhibit {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "UnInhibit";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // cookie
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Throttle {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "Throttle";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // application_name
      "s"  // reason_for_inhibit
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"  // cookie
      ")"};
};

// method
struct UnThrottle {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kMethodName[] = "UnThrottle";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // cookie
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// signal
struct ActiveChanged {
  static constexpr char kInterfaceName[] = "org.freedesktop.ScreenSaver";
  static constexpr char kSignalName[] = "ActiveChanged";
  static constexpr gvariant::Type kType{
      "("
      "b"  // arg_0
      ")"};
};

}  // namespace remoting::org_freedesktop_ScreenSaver

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SCREENSAVER_H_
