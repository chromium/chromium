// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import <vector>

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
  kMaxValue = kSuggestedTab
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

// LINT.IfChange(FocusResultedInNavigationType)
enum class FocusResultedInNavigationType {
  kNoNavigationNoAttachments = 0,
  kNavigationNoAttachments = 1,
  kNoNavigationWithAttachments = 2,
  kNavigationWithAttachments = 3,
  kMaxValue = kNavigationWithAttachments
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FocusResultedInNavigationTypes)

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
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:ComposeboxDragAndDropType)

// Represents the possible attachment types for metrics recording.
enum class ComposeboxMetricsAttachmentType {
  kImage,
  kTab,
  kRawFile,
};

// A metrics recorder object for the composebox.
@interface ComposeboxMetricsRecorder : NSObject
// Records the AI mode activation source.
- (void)recordAiModeActivationSource:(AiModeActivationSource)source;

// Records the type of attachment button shown.
- (void)recordAttachmentButtonShown:(FuseboxAttachmentButtonType)buttonType;

// Records the type of attachment button used.
- (void)recordAttachmentButtonUsed:(FuseboxAttachmentButtonType)buttonType;

// Records the drag and drop type used to attach content to the Composebox
// context.
- (void)recordDragAndDropAttempt:(ComposeboxDragAndDropType)type;

// Records the attachment buttons usage in the composebox session.
- (void)recordAttachmentButtonsUsageInSession;

// Records that the attachments popup was toggled.
- (void)recordAttachmentsMenuShown:(BOOL)shown;

// Records the number of tabs attached from the tab picker.
- (void)recordTabPickerTabsAttached:(NSUInteger)count;

// Records the autocomplete request type when the composebox is abandoned.
- (void)recordAutocompleteRequestTypeAtAbandon:
    (AutocompleteRequestType)requestType;

// Records the autocomplete request type when a navigation occurs.
- (void)recordAutocompleteRequestTypeAtNavigation:
    (AutocompleteRequestType)requestType;

// Records whether a composebox focus resulted in navigation and whether there
// was attachments.
- (void)recordComposeboxFocusResultedInNavigation:(BOOL)navigation
                                  withAttachments:(BOOL)hasAttachments
                                      requestType:
                                          (AutocompleteRequestType)requestType;

// Records the number of attachments of a given type at submission.
- (void)recordAttachCountAtSubmission:(NSUInteger)count
                              forType:(ComposeboxMetricsAttachmentType)type;

// Records the number of images attached.
- (void)recordImagesAttached:(NSUInteger)count;

// Records the number of files attached.
- (void)recordFilesAttached:(NSUInteger)count;

- (void)recordVoiceSearchButtonUsed;
- (void)recordLensSearchButtonUsed;
- (void)recordQRScannerButtonUsed;

// Records that the attachments menu was opened with the given visible buttons.
- (void)recordAttachmentsMenuOpenedWithVisibleButtons:
    (std::vector<FuseboxAttachmentButtonType>)visibleButtons;

// Records whether the user edited the text before entering AI Mode.
- (void)recordTextEditedBeforeAiMode:(BOOL)edited;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_METRICS_COMPOSEBOX_METRICS_RECORDER_H_
