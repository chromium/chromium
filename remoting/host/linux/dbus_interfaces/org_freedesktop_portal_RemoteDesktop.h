// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org_freedesktop_portal_RemoteDesktop.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REMOTEDESKTOP_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REMOTEDESKTOP_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_portal_RemoteDesktop {

// method
struct CreateSession {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
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
struct SelectDevices {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "SelectDevices";
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
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
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
struct NotifyPointerMotion {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyPointerMotion";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "d"      // dx
      "d"      // dy
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerMotionAbsolute {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyPointerMotionAbsolute";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "u"      // stream
      "d"      // x
      "d"      // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerButton {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyPointerButton";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "i"      // button
      "u"      // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerAxis {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyPointerAxis";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "d"      // dx
      "d"      // dy
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerAxisDiscrete {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyPointerAxisDiscrete";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "u"      // axis
      "i"      // steps
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyKeyboardKeycode {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyKeyboardKeycode";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "i"      // keycode
      "u"      // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyKeyboardKeysym {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyKeyboardKeysym";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "i"      // keysym
      "u"      // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchDown {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyTouchDown";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "u"      // stream
      "u"      // slot
      "d"      // x
      "d"      // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchMotion {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyTouchMotion";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "u"      // stream
      "u"      // slot
      "d"      // x
      "d"      // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchUp {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "NotifyTouchUp";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      "u"      // slot
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ConnectToEIS {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kMethodName[] = "ConnectToEIS";
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
struct AvailableDeviceTypes {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kPropertyName[] = "AvailableDeviceTypes";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct version {
  static constexpr char kInterfaceName[] =
      "org.freedesktop.portal.RemoteDesktop";
  static constexpr char kPropertyName[] = "version";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_freedesktop_portal_RemoteDesktop

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_REMOTEDESKTOP_H_
