// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.login1.Session.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_SESSION_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_SESSION_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_login1_Session {

// method
struct Terminate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "Terminate";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Activate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "Activate";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Lock {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "Lock";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Unlock {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "Unlock";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetIdleHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetIdleHint";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // idle
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetLockedHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetLockedHint";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // locked
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Kill {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "Kill";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // whom
      "i"  // signal_number
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct TakeControl {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "TakeControl";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // force
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ReleaseControl {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "ReleaseControl";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetType {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetType";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // type
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetClass {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetClass";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // class
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetDisplay {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetDisplay";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // display
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetTTY {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetTTY";
  static constexpr gvariant::Type kInType{
      "("
      "h"  // tty_fd
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct TakeDevice {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "TakeDevice";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // major
      "u"  // minor
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // fd
      "b"  // inactive
      ")"};
};

// method
struct ReleaseDevice {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "ReleaseDevice";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // major
      "u"  // minor
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct PauseDeviceComplete {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "PauseDeviceComplete";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // major
      "u"  // minor
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetBrightness {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kMethodName[] = "SetBrightness";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // subsystem
      "s"  // name
      "u"  // brightness
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct Id {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Id";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct User {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "User";
  static constexpr gvariant::Type kType{"(uo)"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Name {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Name";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Timestamp {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Timestamp";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct TimestampMonotonic {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "TimestampMonotonic";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct VTNr {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "VTNr";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Seat {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Seat";
  static constexpr gvariant::Type kType{"(so)"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct TTY {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "TTY";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Display {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Display";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Remote {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Remote";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RemoteHost {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "RemoteHost";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RemoteUser {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "RemoteUser";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Service {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Service";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Desktop {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Desktop";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Scope {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Scope";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Leader {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Leader";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Audit {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Audit";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Type {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Type";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Class {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Class";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Active {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "Active";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct State {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "State";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "IdleHint";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleSinceHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "IdleSinceHint";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleSinceHintMonotonic {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "IdleSinceHintMonotonic";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct CanIdle {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "CanIdle";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct CanLock {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "CanLock";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct LockedHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kPropertyName[] = "LockedHint";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct PauseDevice {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kSignalName[] = "PauseDevice";
  static constexpr gvariant::Type kType{
      "("
      "u"  // major
      "u"  // minor
      "s"  // type
      ")"};
};

// signal
struct ResumeDevice {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kSignalName[] = "ResumeDevice";
  static constexpr gvariant::Type kType{
      "("
      "u"  // major
      "u"  // minor
      "h"  // fd
      ")"};
};

// signal
struct LockSignal {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kSignalName[] = "Lock";
  static constexpr gvariant::Type kType{"()"};
};

// signal
struct UnlockSignal {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Session";
  static constexpr char kSignalName[] = "Unlock";
  static constexpr gvariant::Type kType{"()"};
};

}  // namespace remoting::org_freedesktop_login1_Session

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_SESSION_H_
