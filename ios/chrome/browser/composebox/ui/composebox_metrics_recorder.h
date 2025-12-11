// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

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
  kMaxValue = kClipboard
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:FuseboxAttachmentButtonType)

// LINT.IfChange(AutocompleteRequestType)
enum class AutocompleteRequestType {
  kSearch = 0,
  kSearchPrefetch = 1,
  kAIMode = 2,
  kImageGeneration = 3,
  kMaxValue = kImageGeneration
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

// A metrics recorder object for the composebox.
@interface ComposeboxMetricsRecorder : NSObject
// Records the AI mode activation source.
- (void)recordAiModeActivationSource:(AiModeActivationSource)source;

// Records the type of attachment button shown.
- (void)recordAttachmentButtonShown:(FuseboxAttachmentButtonType)buttonType;

// Records the type of attachment button used.
- (void)recordAttachmentButtonUsed:(FuseboxAttachmentButtonType)buttonType;

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

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_METRICS_RECORDER_H_
