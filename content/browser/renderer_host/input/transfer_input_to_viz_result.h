// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TRANSFER_INPUT_TO_VIZ_RESULT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TRANSFER_INPUT_TO_VIZ_RESULT_H_

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
  kMaxValue = kMultipleBrowserWindowsOpen,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:TransferInputToVizResult)

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TRANSFER_INPUT_TO_VIZ_RESULT_H_
