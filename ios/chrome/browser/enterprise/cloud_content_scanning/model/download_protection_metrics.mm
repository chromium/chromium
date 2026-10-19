// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/download_protection_metrics.h"

extern const char kIOSDownloadProtectionScanTriggeredEventResultHistogram[] =
    "Enterprise.IOS.DownloadProtection.ScanTriggered.EventResult";

extern const char
    kIOSDownloadProtectionScanTriggeredWarningBypassedHistogram[] =
        "Enterprise.IOS.DownloadProtection.ScanTriggered.WarningBypassed";

extern const char
    kIOSDownloadProtectionScanTriggeredClonefileSuccessHistogram[] =
        "Enterprise.IOS.DownloadProtection.ScanTriggered.Clonefile.Success";

extern const char
    kIOSDownloadProtectionScanTriggeredTempDirectoryDeleteSuccessHistogram[] =
        "Enterprise.IOS.DownloadProtection.ScanTriggered.TempDirectoryDelete."
        "Success";
