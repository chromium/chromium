// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_CREDENTIAL_IMPORT_STAGE_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_CREDENTIAL_IMPORT_STAGE_H_

// Represents different stages of credential import.
enum class CredentialImportStage {
  // The initial state, importing has not started.
  kNotStarted = 0,
  // The credentials started being imported.
  kImporting = 1,
  // All credentials were imported.
  kImported = 2,
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_CREDENTIAL_IMPORT_STAGE_H_
