// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_

// Logs the steady state (unfocused) omnibox position when starting the app.
// This is used to measure the population for each omnibox position.
extern const char kOmniboxSteadyStatePositionAtStartup[];

// Logs the selected steady state (unfocused) omnibox position when starting the
// app. This only logged when the position is user selected.
extern const char kOmniboxSteadyStatePositionAtStartupSelected[];

// Enum for the IOS.Omnibox.SteadyStatePosition histogram.
// Keep in sync with "OmniboxPositionType"
// in src/tools/metrics/histograms/enums.xml.
enum class OmniboxPositionType {
  kTop = 0,
  kBottom = 1,
  kMaxValue = kBottom,
};

// Logs the device switcher result when starting the app. Device switcher result
// determines if the bottom omnibox is presented by default.
extern const char kOmniboxDeviceSwitcherResultAtStartup[];

// Logs the device switcher result when the users leaves NTP after FRE. Device
// switcher result determines if the bottom omnibox is presented by default.
extern const char kOmniboxDeviceSwitcherResultAtFRE[];

// Enum for IOS.Omnibox.DeviceSwitcherResult.* histograms.
// Keep in sync with "OmniboxDeviceSwitcherResult" in
// src/tools/metrics/histograms/enums.xml.
enum class OmniboxDeviceSwitcherResult {
  kUnknown = 0,        // Not logged
  kUnavailable = 1,    // Result are not ready/unavailable.
  kTopOmnibox = 2,     // Top omnibox should be presented by default.
  kBottomOmnibox = 3,  // Bottom omnibox should be presented by default.
  kNotNewUser =
      4,  // User is not new. (device switcher result are not checked).
  kMaxValue = kNotNewUser,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_
