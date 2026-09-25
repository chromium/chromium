// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/paste_protection_metrics.h"

extern const char kIOSPasteProtectionScanTriggeredEventResultHistogram[] =
    "Enterprise.IOS.PasteProtection.ScanTriggered.EventResult";

extern const char kIOSPasteProtectionScanTriggeredScanTypeHistogram[] =
    "Enterprise.IOS.PasteProtection.ScanTriggered.ScanType";

extern const char kIOSPasteProtectionScanTriggeredWarningBypassedHistogram[] =
    "Enterprise.IOS.PasteProtection.ScanTriggered.WarningBypassed";

extern const char kIOSPasteProtectionScanTriggeredScanTimeHistogram[] =
    "Enterprise.IOS.PasteProtection.ScanTriggered.ScanTime";

extern const char kIOSPasteProtectionScanTriggeredPasteInvalidatedHistogram[] =
    "Enterprise.IOS.PasteProtection.ScanTriggered.PasteInvalidated";

extern const char
    kIOSPasteProtectionScanTriggeredConsecutivePasteBlockedHistogram[] =
        "Enterprise.IOS.PasteProtection.ScanTriggered.ConsecutivePasteBlocked";
