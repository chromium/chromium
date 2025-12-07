// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_CHROMIUM_TESTINTERFACE_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_CHROMIUM_TESTINTERFACE_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_chromium_TestInterface {

// method
struct Echo {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kMethodName[] = "Echo";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // text_message
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // text_message
      ")"};
};

// method
struct SlowEcho {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kMethodName[] = "SlowEcho";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // text_message
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // text_message
      ")"};
};

// method
struct AsyncEcho {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kMethodName[] = "AsyncEcho";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // text_message
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "s"  // text_message
      ")"};
};

// method
// Always fails
struct BrokenMethod {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kMethodName[] = "BrokenMethod";
  static constexpr gvariant::Type kInType{"r"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct PerformAction {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kMethodName[] = "PerformAction";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // action
      "o"  // object_path
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct Name {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kPropertyName[] = "Name";
  static constexpr gvariant::Type kType{"s"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = true;
};

// property
struct Version {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kPropertyName[] = "Version";
  static constexpr gvariant::Type kType{"n"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Methods {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kPropertyName[] = "Methods";
  static constexpr gvariant::Type kType{"as"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Objects {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kPropertyName[] = "Objects";
  static constexpr gvariant::Type kType{"ao"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Bytes {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kPropertyName[] = "Bytes";
  static constexpr gvariant::Type kType{"ay"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct Test {
  static constexpr char kInterfaceName[] = "org.chromium.TestInterface";
  static constexpr char kSignalName[] = "Test";
  static constexpr gvariant::Type kType{
      "("
      "s"  // message
      ")"};
};

}  // namespace remoting::org_chromium_TestInterface

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_CHROMIUM_TESTINTERFACE_H_
