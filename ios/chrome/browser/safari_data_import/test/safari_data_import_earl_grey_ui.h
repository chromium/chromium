/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_

/// Tap "no thanks" to dismiss Safari data import entry point if displayed. If
/// `verify_visibility`, fails the test if the entry point is invisible.
/// TODO(crbug.com/439056937): Remove `verify_visibility` and force
/// verification.
void DismissSafariDataImportEntryPoint(bool verify_visibility);

/// Visibility of Safari data import entry point.
bool IsSafariDataImportEntryPointVisible();

/// Tap "Import" on the Safari data import entry point displayed.
void StartImportOnSafariDataImportEntryPoint();

/// Tap "Remind Me Later" on Safari data import entry point displayed.
void SetReminderOnSafariDataImportEntryPoint();

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_
