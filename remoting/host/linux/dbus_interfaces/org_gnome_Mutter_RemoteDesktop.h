// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.gnome.Mutter.RemoteDesktop.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_REMOTEDESKTOP_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_REMOTEDESKTOP_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting {

namespace org_gnome_Mutter_RemoteDesktop {

// method
struct CreateSession {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.RemoteDesktop";
  static constexpr char kMethodName[] = "CreateSession";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // session_path
      ")"};
};

// method
struct CreateSession_Patched {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.RemoteDesktop";
  static constexpr char kMethodName[] = "CreateSession";
  static constexpr gvariant::Type kInType{"(b)"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // session_path
      ")"};
};

// property
struct SupportedDeviceTypes {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.RemoteDesktop";
  static constexpr char kPropertyName[] = "SupportedDeviceTypes";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Version {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.RemoteDesktop";
  static constexpr char kPropertyName[] = "Version";
  static constexpr gvariant::Type kType{"i"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace org_gnome_Mutter_RemoteDesktop

namespace org_gnome_Mutter_RemoteDesktop_Session {

// method
struct Start {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "Start";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Stop {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "Stop";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyKeyboardKeycode {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyKeyboardKeycode";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // keycode
      "b"  // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyKeyboardKeysym {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyKeyboardKeysym";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // keysym
      "b"  // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerButton {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyPointerButton";
  static constexpr gvariant::Type kInType{
      "("
      "i"  // button
      "b"  // state
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerAxis {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyPointerAxis";
  static constexpr gvariant::Type kInType{
      "("
      "d"  // dx
      "d"  // dy
      "u"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerAxisDiscrete {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyPointerAxisDiscrete";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // axis
      "i"  // steps
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerMotionRelative {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyPointerMotionRelative";
  static constexpr gvariant::Type kInType{
      "("
      "d"  // dx
      "d"  // dy
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyPointerMotionAbsolute {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyPointerMotionAbsolute";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // stream
      "d"  // x
      "d"  // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchDown {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyTouchDown";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // stream
      "u"  // slot
      "d"  // x
      "d"  // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchMotion {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyTouchMotion";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // stream
      "u"  // slot
      "d"  // x
      "d"  // y
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct NotifyTouchUp {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "NotifyTouchUp";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // slot
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct EnableClipboard {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "EnableClipboard";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct DisableClipboard {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "DisableClipboard";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetSelection {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "SetSelection";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SelectionWrite {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "SelectionWrite";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // serial
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// method
struct SelectionWriteDone {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "SelectionWriteDone";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // serial
      "b"  // success
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SelectionRead {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "SelectionRead";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // mime_type
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// method
struct ConnectToEIS {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kMethodName[] = "ConnectToEIS";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// property
struct SessionId {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kPropertyName[] = "SessionId";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct CapsLockState {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kPropertyName[] = "CapsLockState";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct NumLockState {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kPropertyName[] = "NumLockState";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct Closed {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kSignalName[] = "Closed";
  static constexpr gvariant::Type kType{"()"};
};

// signal
struct SelectionOwnerChanged {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kSignalName[] = "SelectionOwnerChanged";
  static constexpr gvariant::Type kType{
      "("
      "a{sv}"  // options
      ")"};
};

// signal
struct SelectionTransfer {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.RemoteDesktop.Session";
  static constexpr char kSignalName[] = "SelectionTransfer";
  static constexpr gvariant::Type kType{
      "("
      "s"  // mime_type
      "u"  // serial
      ")"};
};

}  // namespace org_gnome_Mutter_RemoteDesktop_Session

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_REMOTEDESKTOP_H_
