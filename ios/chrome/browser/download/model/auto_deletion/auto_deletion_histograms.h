// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_HISTOGRAMS_H_

// UMA histogram names.
extern const char kAutoDeletionServiceActionsHistogram[];
extern const char kAutoDeletionServiceFileRemovalFailureHistogram[];

// Enum for the IOS.AutoDeletion.ServiceActions histogram. Keep in sync with
// "AutoDeletionServiceActionsType" in tools/metrics/histograms/enums.xml
// LINT.IfChange
enum class AutoDeletionServiceActions {
  kFileScheduledForAutoDeletion = 0,
  kScheduledFileIdentifiedForRemoval = 1,
  kScheduledFileRemovedFromDevice = 2,
  kMaxValue = kScheduledFileRemovedFromDevice,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

// Enum for the IOS.AutoDeletion.FileRemovalFailure histogram. Keep in sync with
// "AutoDeletionServiceFileRemovalFailureType" in
// tools/metrics/histograms/enums.xml.
// LINT.IfChange
enum class AutoDeletionServiceFileRemovalFailures {
  kHashMismatch = 0,
  kFileDoesNotExist = 1,
  kGenericRemovalError = 2,
  kMaxValue = kGenericRemovalError,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_HISTOGRAMS_H_
