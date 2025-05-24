// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_METRICS_GLIC_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_METRICS_GLIC_METRICS_H_

extern const char kGLICConsentTypeHistogram[];

// Enum for the IOS.GLIC.Outcome histogram.
// Keep in sync with "GLICConsentType"
// LINT.IfChange(GLICConsentType)
enum class GLICConsentType {
  kCancel = 0,
  kDismiss = 1,
  kAccept = 2,
  kMaxValue = kAccept,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:GLICConsentType)

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_METRICS_GLIC_METRICS_H_
