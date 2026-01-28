// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.login1.Manager.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_MANAGER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_MANAGER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_login1_Manager {

// method
struct GetSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "GetSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // object_path
      ")"};
};

// method
struct GetSessionByPID {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "GetSessionByPID";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // pid
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // object_path
      ")"};
};

// method
struct GetUser {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "GetUser";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // uid
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // object_path
      ")"};
};

// method
struct GetUserByPID {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "GetUserByPID";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // pid
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // object_path
      ")"};
};

// method
struct GetSeat {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "GetSeat";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // seat_id
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // object_path
      ")"};
};

// method
struct ListSessions {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ListSessions";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a(susso)"  // sessions
      ")"};
};

// method
struct ListSessionsEx {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ListSessionsEx";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a(sussussbto)"  // sessions
      ")"};
};

// method
struct ListUsers {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ListUsers";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a(uso)"  // users
      ")"};
};

// method
struct ListSeats {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ListSeats";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a(so)"  // seats
      ")"};
};

// method
struct ListInhibitors {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ListInhibitors";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "a(ssssuu)"  // inhibitors
      ")"};
};

// method
struct CreateSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CreateSession";
  static constexpr gvariant::Type kInType{
      "("
      "u"      // uid
      "u"      // pid
      "s"      // service
      "s"      // type
      "s"      // class
      "s"      // desktop
      "s"      // seat_id
      "u"      // vtnr
      "s"      // tty
      "s"      // display
      "b"      // remote
      "s"      // remote_user
      "s"      // remote_host
      "a(sv)"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // session_id
      "o"  // object_path
      "s"  // runtime_path
      "h"  // fifo_fd
      "u"  // uid
      "s"  // seat_id
      "u"  // vtnr
      "b"  // existing
      ")"};
};

// method
struct CreateSessionWithPIDFD {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CreateSessionWithPIDFD";
  static constexpr gvariant::Type kInType{
      "("
      "u"      // uid
      "h"      // pidfd
      "s"      // service
      "s"      // type
      "s"      // class
      "s"      // desktop
      "s"      // seat_id
      "u"      // vtnr
      "s"      // tty
      "s"      // display
      "b"      // remote
      "s"      // remote_user
      "s"      // remote_host
      "t"      // flags
      "a(sv)"  // properties
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // session_id
      "o"  // object_path
      "s"  // runtime_path
      "h"  // fifo_fd
      "u"  // uid
      "s"  // seat_id
      "u"  // vtnr
      "b"  // existing
      ")"};
};

// method
struct ReleaseSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ReleaseSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ActivateSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ActivateSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ActivateSessionOnSeat {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ActivateSessionOnSeat";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      "s"  // seat_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct LockSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "LockSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct UnlockSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "UnlockSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct LockSessions {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "LockSessions";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct UnlockSessions {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "UnlockSessions";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct KillSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "KillSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      "s"  // whom
      "i"  // signal_number
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct KillUser {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "KillUser";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // uid
      "i"  // signal_number
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct TerminateSession {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "TerminateSession";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // session_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct TerminateUser {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "TerminateUser";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // uid
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct TerminateSeat {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "TerminateSeat";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // seat_id
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetUserLinger {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetUserLinger";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // uid
      "b"  // enable
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct AttachDevice {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "AttachDevice";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // seat_id
      "s"  // sysfs_path
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct FlushDevices {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "FlushDevices";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct PowerOff {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "PowerOff";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct PowerOffWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "PowerOffWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Reboot {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Reboot";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct RebootWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "RebootWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Halt {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Halt";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct HaltWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "HaltWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Suspend {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Suspend";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SuspendWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SuspendWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Hibernate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Hibernate";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct HibernateWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "HibernateWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct HybridSleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "HybridSleep";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct HybridSleepWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "HybridSleepWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SuspendThenHibernate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SuspendThenHibernate";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // interactive
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SuspendThenHibernateWithFlags {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SuspendThenHibernateWithFlags";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct Sleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Sleep";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // flags
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct CanPowerOff {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanPowerOff";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanReboot {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanReboot";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanHalt {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanHalt";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanSuspend {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanSuspend";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanHibernate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanHibernate";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanHybridSleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanHybridSleep";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanSuspendThenHibernate {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanSuspendThenHibernate";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct CanSleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanSleep";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct ScheduleShutdown {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "ScheduleShutdown";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // type
      "t"  // usec
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct CancelScheduledShutdown {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CancelScheduledShutdown";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "b"  // cancelled
      ")"};
};

// method
struct Inhibit {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "Inhibit";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // what
      "s"  // who
      "s"  // why
      "s"  // mode
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "h"  // pipe_fd
      ")"};
};

// method
struct CanRebootParameter {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanRebootParameter";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct SetRebootParameter {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetRebootParameter";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // parameter
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct CanRebootToFirmwareSetup {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanRebootToFirmwareSetup";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct SetRebootToFirmwareSetup {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetRebootToFirmwareSetup";
  static constexpr gvariant::Type kInType{
      "("
      "b"  // enable
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct CanRebootToBootLoaderMenu {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanRebootToBootLoaderMenu";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct SetRebootToBootLoaderMenu {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetRebootToBootLoaderMenu";
  static constexpr gvariant::Type kInType{
      "("
      "t"  // timeout
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct CanRebootToBootLoaderEntry {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "CanRebootToBootLoaderEntry";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // result
      ")"};
};

// method
struct SetRebootToBootLoaderEntry {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetRebootToBootLoaderEntry";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // boot_loader_entry
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetWallMessage {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kMethodName[] = "SetWallMessage";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // wall_message
      "b"  // enable
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct EnableWallMessages {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "EnableWallMessages";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = true;
};

// property
struct WallMessage {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "WallMessage";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = true;
};

// property
struct NAutoVTs {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "NAutoVTs";
  static constexpr gvariant::Type kType{"u"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct KillOnlyUsers {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "KillOnlyUsers";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct KillExcludeUsers {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "KillExcludeUsers";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct KillUserProcesses {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "KillUserProcesses";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RebootParameter {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RebootParameter";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RebootToFirmwareSetup {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RebootToFirmwareSetup";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RebootToBootLoaderMenu {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RebootToBootLoaderMenu";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RebootToBootLoaderEntry {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RebootToBootLoaderEntry";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct BootLoaderEntries {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "BootLoaderEntries";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "IdleHint";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleSinceHint {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "IdleSinceHint";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleSinceHintMonotonic {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "IdleSinceHintMonotonic";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct BlockInhibited {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "BlockInhibited";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct BlockWeakInhibited {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "BlockWeakInhibited";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct DelayInhibited {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "DelayInhibited";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct InhibitDelayMaxUSec {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "InhibitDelayMaxUSec";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct UserStopDelayUSec {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "UserStopDelayUSec";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct SleepOperation {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "SleepOperation";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandlePowerKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandlePowerKey";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandlePowerKeyLongPress {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandlePowerKeyLongPress";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleRebootKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleRebootKey";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleRebootKeyLongPress {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleRebootKeyLongPress";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleSuspendKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleSuspendKey";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleSuspendKeyLongPress {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleSuspendKeyLongPress";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleHibernateKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleHibernateKey";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleHibernateKeyLongPress {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleHibernateKeyLongPress";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleLidSwitch {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleLidSwitch";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleLidSwitchExternalPower {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleLidSwitchExternalPower";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleLidSwitchDocked {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleLidSwitchDocked";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HandleSecureAttentionKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HandleSecureAttentionKey";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HoldoffTimeoutUSec {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "HoldoffTimeoutUSec";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleAction {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "IdleAction";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct IdleActionUSec {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "IdleActionUSec";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct PreparingForShutdown {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "PreparingForShutdown";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct PreparingForShutdownWithMetadata {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "PreparingForShutdownWithMetadata";
  static constexpr gvariant::Type kType{"a{sv}"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct PreparingForSleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "PreparingForSleep";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct ScheduledShutdown {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "ScheduledShutdown";
  static constexpr gvariant::Type kType{"(st)"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct DesignatedMaintenanceTime {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "DesignatedMaintenanceTime";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Docked {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "Docked";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct LidClosed {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "LidClosed";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct OnExternalPower {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "OnExternalPower";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RemoveIPC {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RemoveIPC";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RuntimeDirectorySize {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RuntimeDirectorySize";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct RuntimeDirectoryInodesMax {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "RuntimeDirectoryInodesMax";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct InhibitorsMax {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "InhibitorsMax";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct NCurrentInhibitors {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "NCurrentInhibitors";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct SessionsMax {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "SessionsMax";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct NCurrentSessions {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "NCurrentSessions";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct StopIdleSessionUSec {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kPropertyName[] = "StopIdleSessionUSec";
  static constexpr gvariant::Type kType{"t"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct SecureAttentionKey {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "SecureAttentionKey";
  static constexpr gvariant::Type kType{
      "("
      "s"  // seat_id
      "o"  // object_path
      ")"};
};

// signal
struct SessionNew {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "SessionNew";
  static constexpr gvariant::Type kType{
      "("
      "s"  // session_id
      "o"  // object_path
      ")"};
};

// signal
struct SessionRemoved {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "SessionRemoved";
  static constexpr gvariant::Type kType{
      "("
      "s"  // session_id
      "o"  // object_path
      ")"};
};

// signal
struct UserNew {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "UserNew";
  static constexpr gvariant::Type kType{
      "("
      "u"  // uid
      "o"  // object_path
      ")"};
};

// signal
struct UserRemoved {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "UserRemoved";
  static constexpr gvariant::Type kType{
      "("
      "u"  // uid
      "o"  // object_path
      ")"};
};

// signal
struct SeatNew {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "SeatNew";
  static constexpr gvariant::Type kType{
      "("
      "s"  // seat_id
      "o"  // object_path
      ")"};
};

// signal
struct SeatRemoved {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "SeatRemoved";
  static constexpr gvariant::Type kType{
      "("
      "s"  // seat_id
      "o"  // object_path
      ")"};
};

// signal
struct PrepareForShutdown {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "PrepareForShutdown";
  static constexpr gvariant::Type kType{
      "("
      "b"  // start
      ")"};
};

// signal
struct PrepareForShutdownWithMetadata {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "PrepareForShutdownWithMetadata";
  static constexpr gvariant::Type kType{
      "("
      "b"      // start
      "a{sv}"  // metadata
      ")"};
};

// signal
struct PrepareForSleep {
  static constexpr char kInterfaceName[] = "org.freedesktop.login1.Manager";
  static constexpr char kSignalName[] = "PrepareForSleep";
  static constexpr gvariant::Type kType{
      "("
      "b"  // start
      ")"};
};

}  // namespace remoting::org_freedesktop_login1_Manager

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_LOGIN1_MANAGER_H_
