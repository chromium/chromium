// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_ENTRY_POINT_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_ENTRY_POINT_H_

class ProfileIOS;

// Enums for entry point for Safari import half-sheet landing page, and
// available user actions on the page.
enum class SafariDataImportEntryPoint { kFirstRun, kReminder, kSetting };

// Whether the user should see the import from Safari workflow.
bool ShouldShowSafariDataImportEntryPoint(ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_ENTRY_POINT_H_
