// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.gnome.Mutter.ScreenCast.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_SCREENCAST_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_SCREENCAST_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting {

namespace org_gnome_Mutter_ScreenCast {

// method
struct CreateSession {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast";
  static constexpr char kMethodName[] = "CreateSession";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // session_path
      ")"};
};

// property
struct Version {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast";
  static constexpr char kPropertyName[] = "Version";
  static constexpr gvariant::Type kType{"i"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace org_gnome_Mutter_ScreenCast

namespace org_gnome_Mutter_ScreenCast_Session {

// method
struct Start {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "Start";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Stop {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "Stop";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct RecordMonitor {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "RecordMonitor";
  static constexpr gvariant::Type kInType{
      "("
      "s"      // connector
      "a{sv}"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // stream_path
      ")"};
};

// method
struct RecordWindow {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "RecordWindow";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // stream_path
      ")"};
};

// method
struct RecordArea {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "RecordArea";
  static constexpr gvariant::Type kInType{
      "("
      "i"      // x
      "i"      // y
      "i"      // width
      "i"      // height
      "a{sv}"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // stream_path
      ")"};
};

// method
struct RecordVirtual {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kMethodName[] = "RecordVirtual";
  static constexpr gvariant::Type kInType{
      "("
      "a{sv}"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // stream_path
      ")"};
};

// signal
struct Closed {
  static constexpr char kInterfaceName[] =
      "org.gnome.Mutter.ScreenCast.Session";
  static constexpr char kSignalName[] = "Closed";
  static constexpr gvariant::Type kType{"()"};
};

}  // namespace org_gnome_Mutter_ScreenCast_Session

namespace org_gnome_Mutter_ScreenCast_Stream {

// method
struct Start {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast.Stream";
  static constexpr char kMethodName[] = "Start";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Stop {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast.Stream";
  static constexpr char kMethodName[] = "Stop";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct Parameters {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast.Stream";
  static constexpr char kPropertyName[] = "Parameters";
  static constexpr gvariant::Type kType{"a{sv}"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct PipeWireStreamAdded {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.ScreenCast.Stream";
  static constexpr char kSignalName[] = "PipeWireStreamAdded";
  static constexpr gvariant::Type kType{
      "("
      "u"  // node_id
      ")"};
};

}  // namespace org_gnome_Mutter_ScreenCast_Stream

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_SCREENCAST_H_
