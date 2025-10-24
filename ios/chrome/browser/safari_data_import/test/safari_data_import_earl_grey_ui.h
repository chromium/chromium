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

/// Action button on the import screen with text.
id<GREYMatcher> ImportScreenButtonWithTextId(int text_id);

/// Load selected file. Returns either when the file is ready to import or when
/// loading fails.
void LoadFile(SafariDataImportTestFile file);

/// Perform assertions on the number of rows in the data item table. If the
/// table should not be visible, `expected_count` should be 0.
void ExpectImportTableHasRowCount(int expected_count);

/// Verify that the cell at `index` in the password conflict screen is
/// selected/not selected.
void ExpectPasswordConflictCellAtIndexSelected(int idx, bool selected);

/// Tap the "info" button to display invalid passwords. Fail if there is no
/// invalid passwords. `imported` is the number of successfully imported
/// passwords, `failed` the number of failed imports.
void TapInfoButtonForInvalidPasswords(int imported, int failed);

/// Exits the import workflow after file is imported.
void CompletesImportWorkflow();

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_EARL_GREY_UI_H_
