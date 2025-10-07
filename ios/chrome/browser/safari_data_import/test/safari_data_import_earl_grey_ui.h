/// Copyright 2025 The Chromium Authors
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_

@protocol GREYMatcher;
enum class SafariDataImportTestFile;

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

/// Taps all the way to the import screen.
void GoToImportScreen();

/// Load selected file. Returns either when the file is ready to import or when
/// loading fails.
void LoadFile(SafariDataImportTestFile file);

/// Perform assertions on the number of rows in the data item table. If the
/// table should not be visible, `expected_count` should be 0.
void ExpectImportTableHasRowCount(int expected_count);

/// Taps the "Import" button to import the loaded file.
void ImportLoadedFile();

/// Wait for import completes.
void WaitForImportCompletes();

/// Exits the import workflow after file is imported.
void CompletesImportWorkflow();

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_
