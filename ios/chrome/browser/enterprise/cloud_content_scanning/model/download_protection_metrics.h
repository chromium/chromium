// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_DOWNLOAD_PROTECTION_METRICS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_DOWNLOAD_PROTECTION_METRICS_H_

// LINT.IfChange(EnterpriseDownloadProtectionEventResult)
enum class EnterpriseDownloadProtectionEventResult {
  // Show that an issue was found and the download is Warned.
  kWarn = 0,
  // Show that the scan failed and the download is blocked.
  kBlock = 1,
  kMaxValue = kBlock
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.EnterpriseDownloadProtectionEventResult)

extern const char kIOSDownloadProtectionScanTriggeredEventResultHistogram[];

extern const char kIOSDownloadProtectionScanTriggeredWarningBypassedHistogram[];

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_DOWNLOAD_PROTECTION_METRICS_H_
