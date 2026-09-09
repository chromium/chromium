// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_CONSTANTS_H_

// LINT.IfChange(AiModeActivationSource)
enum class AiModeActivationSource {
  kToolMenu = 0,
  kDedicatedButton = 1,
  kNTPButton = 2,
  kImplicit = 3,
  kMaxValue = kImplicit,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AiModeActivationSource)

// LINT.IfChange(FuseboxAttachmentButtonType)
enum class FuseboxAttachmentButtonType {
  kCurrentTab = 0,
  kTabPicker = 1,
  kCamera = 2,
  kGallery = 3,
  kFiles = 4,
  kClipboard = 5,
  kSuggestedTab = 6,
  kRecentTab = 7,
  kDriveFiles = 8,
  kMaxValue = kDriveFiles
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxAttachmentButtonType)

// LINT.IfChange(AutocompleteRequestType)
enum class AutocompleteRequestType {
  kSearch = 0,
  kSearchPrefetch = 1,
  kAIMode = 2,
  kImageGeneration = 3,
  kCanvas = 4,
  kDeepSearch = 5,
  kMaxValue = kDeepSearch
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:AutocompleteRequestType)

// LINT.IfChange(ComposeboxDragAndDropType)
enum class ComposeboxDragAndDropType {
  kText = 0,
  kImage = 1,
  kTab = 2,
  kPDF = 3,
  kUnknown = 4,
  kRawFile = 5,
  kMaxValue = kRawFile,
};
// LINT.ThenChange(
//     //tools/metrics/histograms/metadata/omnibox/enums.xml:ComposeboxDragAndDropType,
//     //tools/metrics/histograms/metadata/omnibox/histograms.xml:ComposeboxDragAndDropType
// )

// Represents the possible attachment types for metrics recording.
enum class ComposeboxMetricsAttachmentType {
  kImage,
  kTab,
  kRawFile,
};

// LINT.IfChange(MobileFuseboxPickerAttachmentType)
enum class MobileFuseboxPickerAttachmentType {
  kDrive,
  kGallery,
  kCamera,
  kFile,
  kTabs,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/histograms.xml:MobileFuseboxPickerAttachmentType)

// LINT.IfChange(MobileFuseboxPickerOutcome)
enum class MobileFuseboxPickerOutcome {
  kAttachmentAdded = 0,
  kManualUserExit = 1,
  kPermissionDenied = 2,
  kLocalError = 3,
  kMaxValue = kLocalError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:MobileFuseboxPickerOutcome)

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_CONSTANTS_H_
