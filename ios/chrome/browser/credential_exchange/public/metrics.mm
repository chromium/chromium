// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/public/metrics.h"

#import "base/metrics/histogram_functions.h"

void LogCredentialExportScreenAction(CredentialExportScreenAction action) {
  base::UmaHistogramEnumeration(kCredentialExportScreenActionHistogram, action);
}
