// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_metrics_recorder.h"

#import "base/metrics/histogram_functions.h"

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

}  // namespace

@implementation ComposeboxMetricsRecorder

- (void)recordAiModeActivationSource:(AiModeActivationSource)source {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AiModeActivationSource",
                                source);
}

- (void)recordAttachmentButtonShown:(FuseboxAttachmentButtonType)buttonType {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AttachmentButtonShown",
                                buttonType);
}

- (void)recordAttachmentButtonUsed:(FuseboxAttachmentButtonType)buttonType {
  base::UmaHistogramEnumeration("Omnibox.MobileFusebox.AttachmentButtonUsed",
                                buttonType);
}

- (void)recordAttachmentButtonUsedInSession:
    (FuseboxAttachmentButtonType)buttonType {
  std::string histogram_name =
      "Omnibox.MobileFusebox.AttachmentButtonUsedInSession.";
  histogram_name += GetStringForAttachmentType(buttonType);
  base::UmaHistogramBoolean(histogram_name, true);
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

@end
