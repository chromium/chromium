// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTE_PROTECTION_METRICS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTE_PROTECTION_METRICS_H_

// LINT.IfChange(EnterprisePasteProtectionEventResult)
enum class EnterprisePasteProtectionEventResult {
  // The paste is Allowed.
  kAllow = 0,
  // An issue was found and the paste is Warned.
  kWarn = 1,
  // The scan failed and the paste is blocked.
  kBlock = 2,
  kMaxValue = kBlock
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.EnterpriseDownloadProtectionEventResult)

// LINT.IfChange(EnterprisePasteProtectionScanType)
enum class EnterprisePasteProtectionScanType {
  // Pasteboard content is text.
  kText = 0,
  // Pasteboard content is image.
  kImage = 1,
  kMaxValue = kImage
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.EnterprisePasteProtectionScanType)

// LINT.IfChange(EnterprisePasteProtectionInvalidatedType)
enum class EnterprisePasteProtectionInvalidatedType {
  // Pasteboard Content changed.
  kPasteboardChanged = 0,
  // Navigated away.
  kNavigatedAway = 1,
  // Tab was hidden or switched.
  kInvalidTabState = 2,
  // Scan spinning overlay was interrupted and dismissed by other overlays.
  kSpinningOverlayInterrupted = 3,
  kMaxValue = kSpinningOverlayInterrupted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.EnterprisePasteProtectionInvalidatedType)

extern const char kIOSPasteProtectionScanTriggeredEventResultHistogram[];

extern const char kIOSPasteProtectionScanTriggeredScanTypeHistogram[];

extern const char kIOSPasteProtectionScanTriggeredWarningBypassedHistogram[];

extern const char kIOSPasteProtectionScanTriggeredScanTimeHistogram[];

// TODO(crbug.com/539938728): Add the rest of the invalid paste metrics in tab
// helper after spinning overlay is merged.
extern const char kIOSPasteProtectionScanTriggeredPasteInvalidatedHistogram[];

extern const char
    kIOSPasteProtectionScanTriggeredConsecutivePasteBlockedHistogram[];

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_PASTE_PROTECTION_METRICS_H_
