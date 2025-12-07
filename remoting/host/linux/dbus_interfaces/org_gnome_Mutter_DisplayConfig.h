// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated from org.gnome.Mutter.DisplayConfig.xml

#ifndef REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_DISPLAYCONFIG_H_
#define REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_DISPLAYCONFIG_H_

#include "remoting/host/linux/gvariant_type.h"

namespace remoting::org_gnome_Mutter_DisplayConfig {

// method
struct GetResources {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "GetResources";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"                   // serial
      "a(uxiiiiiuaua{sv})"  // crtcs
      "a(uxiausauaua{sv})"  // outputs
      "a(uxuudu)"           // modes
      "i"                   // max_screen_width
      "i"                   // max_screen_height
      ")"};
};

// method
struct ApplyConfiguration {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "ApplyConfiguration";
  static constexpr gvariant::Type kInType{
      "("
      "u"                // serial
      "b"                // persistent
      "a(uiiiuaua{sv})"  // crtcs
      "a(ua{sv})"        // outputs
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ChangeBacklight {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "ChangeBacklight";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // serial
      "u"  // output
      "i"  // value
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "i"  // new_value
      ")"};
};

// method
struct SetBacklight {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "SetBacklight";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // serial
      "s"  // connector
      "i"  // value
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetLuminance {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "SetLuminance";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // connector
      "u"  // color_mode
      "d"  // luminance
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct ResetLuminance {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "ResetLuminance";
  static constexpr gvariant::Type kInType{
      "("
      "s"  // connector
      "u"  // color_mode
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct GetCrtcGamma {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "GetCrtcGamma";
  static constexpr gvariant::Type kInType{
      "("
      "u"  // serial
      "u"  // crtc
      ")"};
  static constexpr gvariant::Type kOutType{
      "("
      "aq"  // red
      "aq"  // green
      "aq"  // blue
      ")"};
};

// method
struct SetCrtcGamma {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "SetCrtcGamma";
  static constexpr gvariant::Type kInType{
      "("
      "u"   // serial
      "u"   // crtc
      "aq"  // red
      "aq"  // green
      "aq"  // blue
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct GetCurrentState {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "GetCurrentState";
  static constexpr gvariant::Type kInType{"()"};
  static constexpr gvariant::Type kOutType{
      "("
      "u"                              // serial
      "a((ssss)a(siiddada{sv})a{sv})"  // monitors
      "a(iiduba(ssss)a{sv})"           // logical_monitors
      "a{sv}"                          // properties
      ")"};
};

// method
struct ApplyMonitorsConfig {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "ApplyMonitorsConfig";
  static constexpr gvariant::Type kInType{
      "("
      "u"                   // serial
      "u"                   // method
      "a(iiduba(ssa{sv}))"  // logical_monitors
      "a{sv}"               // properties
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// method
struct SetOutputCTM {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kMethodName[] = "SetOutputCTM";
  static constexpr gvariant::Type kInType{
      "("
      "u"            // serial
      "u"            // output
      "(ttttttttt)"  // ctm
      ")"};
  static constexpr gvariant::Type kOutType{"()"};
};

// property
struct Backlight {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "Backlight";
  static constexpr gvariant::Type kType{"(uaa{sv})"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct Luminance {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "Luminance";
  static constexpr gvariant::Type kType{"aa{sv}"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct PowerSaveMode {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "PowerSaveMode";
  static constexpr gvariant::Type kType{"i"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = true;
};

// property
struct PanelOrientationManaged {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "PanelOrientationManaged";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct ApplyMonitorsConfigAllowed {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "ApplyMonitorsConfigAllowed";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct NightLightSupported {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "NightLightSupported";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// property
struct HasExternalMonitor {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kPropertyName[] = "HasExternalMonitor";
  static constexpr gvariant::Type kType{"b"};
  static constexpr bool kReadable = true;
  static constexpr bool kWritable = false;
};

// signal
struct MonitorsChanged {
  static constexpr char kInterfaceName[] = "org.gnome.Mutter.DisplayConfig";
  static constexpr char kSignalName[] = "MonitorsChanged";
  static constexpr gvariant::Type kType{"()"};
};

}  // namespace remoting::org_gnome_Mutter_DisplayConfig

#endif  // REMOTING_HOST_LINUX_DBUS_INTERFACES_ORG_GNOME_MUTTER_DISPLAYCONFIG_H_
