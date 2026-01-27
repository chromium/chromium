// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_METRICS_H_

inline constexpr char kCredentialExportScreenActionHistogram[] =
    "IOS.CredentialExchange.CredentialExportScreenAction";

// LINT.IfChange(CredentialExportScreenAction)
enum class CredentialExportScreenAction {
  kDeselectAllPressed = 0,
  kSelectAllPressed = 1,
  kDownloadToCSVPressed = 2,
  kContinuePressed = 3,
  kMaxValue = kContinuePressed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:CredentialExportScreenAction)

// Records user actions on the export screen.
void LogCredentialExportScreenAction(CredentialExportScreenAction action);

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_PUBLIC_METRICS_H_
