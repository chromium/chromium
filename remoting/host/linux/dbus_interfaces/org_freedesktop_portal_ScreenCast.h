// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org_freedesktop_portal_ScreenCast.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SCREENCAST_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SCREENCAST_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_portal_ScreenCast {

// method
struct CreateSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kMethodName[] = "CreateSession";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // handle
      ")"};
};

// method
struct SelectSources {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kMethodName[] = "SelectSources";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // handle
      ")"};
};

// method
struct Start {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kMethodName[] = "Start";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "s"      // parent_window
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // handle
      ")"};
};

// method
struct OpenPipeWireRemote {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kMethodName[] = "OpenPipeWireRemote";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// property
struct AvailableSourceTypes {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kPropertyName[] = "AvailableSourceTypes";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct AvailableCursorModes {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kPropertyName[] = "AvailableCursorModes";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct version {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.ScreenCast";
  static constexpr char kPropertyName[] = "version";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_freedesktop_portal_ScreenCast

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_SCREENCAST_H_
