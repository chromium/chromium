// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.portal.Clipboard.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_CLIPBOARD_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_CLIPBOARD_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_portal_Clipboard {

// method
struct RequestClipboard {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kMethodName[] = "RequestClipboard";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetSelection {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kMethodName[] = "SetSelection";
  static constexpr gvariant::Type kInType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SelectionWrite {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kMethodName[] = "SelectionWrite";
  static constexpr gvariant::Type kInType{
      "("
      "o"  // session_handle
      "u"  // serial
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// method
struct SelectionWriteDone {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kMethodName[] = "SelectionWriteDone";
  static constexpr gvariant::Type kInType{
      "("
      "o"  // session_handle
      "u"  // serial
      "b"  // success
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SelectionRead {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kMethodName[] = "SelectionRead";
  static constexpr gvariant::Type kInType{
      "("
      "o"  // session_handle
      "s"  // mime_type
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      ")"};
};

// property
struct version {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kPropertyName[] = "version";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct SelectionOwnerChanged {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kSignalName[] = "SelectionOwnerChanged";
  static constexpr gvariant::Type kType{
      "("
      "o"      // session_handle
      "a{sv}"  // options
      ")"};
};

// signal
struct SelectionTransfer {
  static constexpr char kInterfaceName[] = "org.freedesktop.portal.Clipboard";
  static constexpr char kSignalName[] = "SelectionTransfer";
  static constexpr gvariant::Type kType{
      "("
      "o"  // session_handle
      "s"  // mime_type
      "u"  // serial
      ")"};
};

}  // namespace remoting::org_freedesktop_portal_Clipboard

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_PORTAL_CLIPBOARD_H_
