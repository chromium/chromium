// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_STAGE_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_STAGE_H_

// Enum for the different stages of the Safari data import process.
enum class SafariDataImportStage {
  // The initial state where no import has been started.
  kNotStarted = 1,
  // The user has selected a file and the import is in progress.
  kFileLoading = 2,
  // The file has been loaded and is ready to be imported.
  kReadyForImport = 3,
  // The data is being imported.
  kImporting = 4,
  // The data has been imported.
  kImported = 5,
};

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_STAGE_H_
