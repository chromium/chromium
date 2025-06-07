// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_TRANSFER_INPUT_TO_VIZ_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_TRANSFER_INPUT_TO_VIZ_RESULT_H_

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
// LINT.IfChange(TransferInputToVizResult)
enum class TransferInputToVizResult {
  kSuccessfullyTransferred = 0,
  kInputTransferHandlerNotFoundInMap = 1,
  kNonFingerToolType = 2,
  kVizInitializationNotComplete = 3,
  kSelectionHandlesActive = 4,
  kCanTriggerBackGesture = 5,
  kImeIsActive = 6,
  kRequestedByEmbedder = 7,
  kSystemServerDidNotTransfer = 8,
  kBrowserTokenChanged = 9,
  kMultipleBrowserWindowsOpen = 10,
  kDownTimeAfterEventTime = 11,
  kSequenceTransferredBackFromViz = 12,
  kWebContentsIgnoringInputEvents = 13,
  kRIRDelegateConnectionNotSetup = 14,
  kMaxValue = kRIRDelegateConnectionNotSetup,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TransferInputToVizResult)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_TRANSFER_INPUT_TO_VIZ_RESULT_H_
