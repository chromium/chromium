// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_metrics_recorder.h"

#import <set>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"

namespace {

/// Returns a string mapping to the attachmentType.
std::string GetStringForAttachmentType(
    FuseboxAttachmentButtonType attachmentType) {
  switch (attachmentType) {
    case FuseboxAttachmentButtonType::kCurrentTab:
      return "CurrentTab";
    case FuseboxAttachmentButtonType::kTabPicker:
      return "TabPicker";
    case FuseboxAttachmentButtonType::kCamera:
      return "Camera";
    case FuseboxAttachmentButtonType::kGallery:
      return "Gallery";
    case FuseboxAttachmentButtonType::kFiles:
      return "Files";
    case FuseboxAttachmentButtonType::kClipboard:
      return "Clipboard";
    default:
      return "";
  }
}

/// Returns a string mapping to the drag and drop type.
std::string GetStringForDragAndDropType(ComposeboxDragAndDropType type) {
  switch (type) {
    case ComposeboxDragAndDropType::kText:
      return ".Text";
    case ComposeboxDragAndDropType::kImage:
      return ".Image";
    case ComposeboxDragAndDropType::kTab:
      return ".Tab";
    case ComposeboxDragAndDropType::kPDF:
      return ".PDF";
    case ComposeboxDragAndDropType::kUnknown:
      return ".Unknown";
  }
  NOTREACHED();
}

}  // namespace

@implementation ComposeboxMetricsRecorder {
  // Set of attachment button types used in the current session.
  std::set<int> _usedAttachmentButtonTypes;
}

- (void)recordAiModeActivationSource:(AiModeActivationSource)source {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AiModeActivationSource",
                                source);
}

- (void)recordAttachmentButtonShown:(FuseboxAttachmentButtonType)buttonType {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AttachmentButtonShown",
                                buttonType);
}

- (void)recordAttachmentButtonsUsageInSession {
  for (int i = 0; i <= static_cast<int>(FuseboxAttachmentButtonType::kMaxValue);
       ++i) {
    FuseboxAttachmentButtonType buttonType =
        static_cast<FuseboxAttachmentButtonType>(i);
    BOOL used =
        _usedAttachmentButtonTypes.contains(static_cast<int>(buttonType));
    [self recordAttachmentButtonUsedInSession:buttonType used:used];
  }
}

- (void)recordAttachmentButtonUsed:(FuseboxAttachmentButtonType)buttonType {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AttachmentButtonUsed",
                                buttonType);
  _usedAttachmentButtonTypes.insert(static_cast<int>(buttonType));
}

- (void)recordDragAndDropAttempt:(ComposeboxDragAndDropType)type {
  std::string histogram_name = "Omnibox.MobileFusebox.DragAndDrop";
  histogram_name += GetStringForDragAndDropType(type);
  base::UmaHistogramEnumeration(histogram_name, type);
}

- (void)recordAttachmentsMenuShown:(BOOL)shown {
  base::UmaHistogramBoolean("Omnibox.MobileFusebox.AttachmentsPopupToggled",
                            shown);
}

- (void)recordAutocompleteRequestTypeAtAbandon:
    (AutocompleteRequestType)requestType {
  base::UmaHistogramEnumeration(
      "Omnibox.MobileFusebox.AutocompleteRequestTypeAtAbandon", requestType);
}

- (void)recordAutocompleteRequestTypeAtNavigation:
    (AutocompleteRequestType)requestType {
  base::UmaHistogramEnumeration(
      "Omnibox.MobileFusebox.AutocompleteRequestTypeAtNavigation", requestType);
}

- (void)recordTabPickerTabsAttached:(NSUInteger)count {
  base::UmaHistogramCounts100("Omnibox.MobileFusebox.TabPickerTabsAttached",
                              count);
}

- (void)recordComposeboxFocusResultedInNavigation:(BOOL)navigation
                                  withAttachments:(BOOL)hasAttachments
                                      requestType:
                                          (AutocompleteRequestType)requestType {
  FocusResultedInNavigationType type;
  if (navigation) {
    type = hasAttachments
               ? FocusResultedInNavigationType::kNavigationWithAttachments
               : FocusResultedInNavigationType::kNavigationNoAttachments;
  } else {
    type = hasAttachments
               ? FocusResultedInNavigationType::kNoNavigationWithAttachments
               : FocusResultedInNavigationType::kNoNavigationNoAttachments;
  }
  base::UmaHistogramEnumeration("Omnibox.FocusResultedInNavigation", type);

  std::string suffix;
  switch (requestType) {
    case AutocompleteRequestType::kSearch:
      suffix = ".Search";
      break;
    case AutocompleteRequestType::kAIMode:
      suffix = ".AIMode";
      break;
    case AutocompleteRequestType::kImageGeneration:
      suffix = ".ImageGeneration";
      break;
    case AutocompleteRequestType::kCanvas:
      suffix = ".Canvas";
      break;
    case AutocompleteRequestType::kDeepSearch:
      suffix = ".DeepSearch";
      break;
    default:
      break;
  }

  if (!suffix.empty()) {
    base::UmaHistogramEnumeration("Omnibox.FocusResultedInNavigation" + suffix,
                                  type);
  }
}

- (void)recordVoiceSearchButtonUsed {
  base::RecordAction(
      base::UserMetricsAction("IOS.Omnibox.MobileFusebox.Action.VoiceSearch"));
}

- (void)recordLensSearchButtonUsed {
  base::RecordAction(
      base::UserMetricsAction("IOS.Omnibox.MobileFusebox.Action.LensSearch"));
}

- (void)recordQRScannerButtonUsed {
  base::RecordAction(
      base::UserMetricsAction("IOS.Omnibox.MobileFusebox.Action.QRScanner"));
}

#pragma mark - private

- (void)recordAttachmentButtonUsedInSession:
            (FuseboxAttachmentButtonType)buttonType
                                       used:(BOOL)used {
  std::string histogram_name =
      "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.";
  histogram_name += GetStringForAttachmentType(buttonType);
  base::UmaHistogramBoolean(histogram_name, used);
}

@end
