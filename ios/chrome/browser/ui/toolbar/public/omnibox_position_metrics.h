// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_

// Logs the steady state (unfocused) omnibox position when starting the app.
// This is used to measure the population for each omnibox position.
extern const char kOmniboxSteadyStatePositionAtStartup[];

// Enum for the IOS.Omnibox.SteadyStatePosition histogram.
// Keep in sync with "OmniboxPositionType"
// in src/tools/metrics/histograms/enums.xml.
enum class OmniboxPositionType {
  kTop = 0,
  kBottom = 1,
  kMaxValue = kBottom,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_POSITION_METRICS_H_
