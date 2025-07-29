// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UTILS_AI_HUB_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UTILS_AI_HUB_METRICS_H_

// Enum for the IOS.AIHub.Action histogram.
// LINT.IfChange(IOSAIHubAction)
enum class IOSAIHubAction {
  kLens = 0,
  kReaderMode = 1,
  kGemini = 2,
  kDismiss = 3,
  kReaderModeOptions = 4,
  kMaxValue = kReaderModeOptions,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSAIHubAction)

// Records the user's selection of an item in the AI Hub.
void RecordAIHubAction(IOSAIHubAction action);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UTILS_AI_HUB_METRICS_H_
