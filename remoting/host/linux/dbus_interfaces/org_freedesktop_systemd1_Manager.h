// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.freedesktop.systemd1.Manager.xml and
// manually edited to remove unneeded parts.

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_MANAGER_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_MANAGER_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_freedesktop_systemd1_Manager {

// method
struct GetUnit {
  static constexpr char kInterfaceName[] = "org.freedesktop.systemd1.Manager";
  static constexpr char kMethodName[] = "GetUnit";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // name
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "o"  // unit
      ")"};
};

// method
struct ListUnitsByPatterns {
  static constexpr char kInterfaceName[] = "org.freedesktop.systemd1.Manager";
  static constexpr char kMethodName[] = "ListUnitsByPatterns";
  static constexpr gvariant::Type kInType{
      "("
      "as"  // states
      "as"  // patterns
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      // Array of (name, description, load_state, active_state, sub_state,
      // following, unit_path, job_id, job_type, job_path).
      "a(ssssssouso)"  // units
      ")"};
};

// property
struct Environment {
  static constexpr char kInterfaceName[] = "org.freedesktop.systemd1.Manager";
  static constexpr char kPropertyName[] = "Environment";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

}  // namespace remoting::org_freedesktop_systemd1_Manager

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_FREEDESKTOP_SYSTEMD1_MANAGER_H_
