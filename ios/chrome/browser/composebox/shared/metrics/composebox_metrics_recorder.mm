// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"

#import <set>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"

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
    case ComposeboxDragAndDropType::kRawFile:
      return ".RawFile";
    case ComposeboxDragAndDropType::kUnknown:
      return ".Unknown";
  }
  NOTREACHED();
}

/// Returns a string mapping to the input item type.
std::string GetStringForInputItemType(ComposeboxMetricsAttachmentType type) {
  switch (type) {
    case ComposeboxMetricsAttachmentType::kTab:
      return "Tab";
    case ComposeboxMetricsAttachmentType::kImage:
      return "Image";
    case ComposeboxMetricsAttachmentType::kRawFile:
      return "File";
  }
}

/// Converts FuseboxAttachmentButtonType to ContextualSearchAttachmentButtonType
contextual_search::ContextualSearchAttachmentButtonType
ContextualSearchAttachmentButtonTypeFromFuseboxButtonType(
    FuseboxAttachmentButtonType buttonType) {
  switch (buttonType) {
    case FuseboxAttachmentButtonType::kCurrentTab:
      return contextual_search::ContextualSearchAttachmentButtonType::
          kCurrentTab;
    case FuseboxAttachmentButtonType::kTabPicker:
      return contextual_search::ContextualSearchAttachmentButtonType::
          kTabPicker;
    case FuseboxAttachmentButtonType::kCamera:
      return contextual_search::ContextualSearchAttachmentButtonType::kCamera;
    case FuseboxAttachmentButtonType::kGallery:
      return contextual_search::ContextualSearchAttachmentButtonType::kGallery;
    case FuseboxAttachmentButtonType::kFiles:
      return contextual_search::ContextualSearchAttachmentButtonType::kFiles;
    case FuseboxAttachmentButtonType::kClipboard:
      return contextual_search::ContextualSearchAttachmentButtonType::
          kClipboard;
    case FuseboxAttachmentButtonType::kSuggestedTab:
      return contextual_search::ContextualSearchAttachmentButtonType::
          kSuggestedTab;
  }
}

/// Converts ComposeboxMetricsAttachmentType to lens::MimeType.
lens::MimeType MimeTypeFromMetricsAttachmentType(
    ComposeboxMetricsAttachmentType type) {
  switch (type) {
    case ComposeboxMetricsAttachmentType::kTab:
      return lens::MimeType::kAnnotatedPageContent;
    case ComposeboxMetricsAttachmentType::kImage:
      return lens::MimeType::kImage;
    case ComposeboxMetricsAttachmentType::kRawFile:
      return lens::MimeType::kPdf;
  }
}

/// Converts ComposeboxMode to omnibox::ToolMode.
omnibox::ToolMode ToolModeFromComposeboxMode(ComposeboxMode tool) {
  switch (tool) {
    case ComposeboxMode::kCanvas:
      return omnibox::ToolMode::TOOL_MODE_CANVAS;
    case ComposeboxMode::kDeepSearch:
      return omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH;
    case ComposeboxMode::kImageGeneration:
      return omnibox::ToolMode::TOOL_MODE_IMAGE_GEN;
    case ComposeboxMode::kAIM:
      return omnibox::ToolMode::TOOL_MODE_AIM;
    case ComposeboxMode::kRegularSearch:
      return omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  }
}

/// Converts ComposeboxModelOption to omnibox::ModelMode.
omnibox::ModelMode ModelModeFromComposeboxModelOption(
    ComposeboxModelOption model) {
  switch (model) {
    case ComposeboxModelOption::kNone:
      return omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
    case ComposeboxModelOption::kRegular:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR;
    case ComposeboxModelOption::kAuto:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE;
    case ComposeboxModelOption::kThinking:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO;
    case ComposeboxModelOption::kThinkingNoGenUI:
      return omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_NO_GEN_UI;
  }
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
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordAttachmentButtonShown(
        ContextualSearchAttachmentButtonTypeFromFuseboxButtonType(buttonType));
  }
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
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordAttachmentButtonUsed(
        ContextualSearchAttachmentButtonTypeFromFuseboxButtonType(buttonType));
  }
}

- (void)recordDragAndDropAttempt:(ComposeboxDragAndDropType)type {
  std::string histogram_name = "Omnibox.MobileFusebox.DragAndDrop";
  histogram_name += GetStringForDragAndDropType(type);
  base::UmaHistogramEnumeration(histogram_name, type);
}

- (void)recordAttachmentsMenuShown:(BOOL)shown {
  base::UmaHistogramBoolean("Omnibox.MobileFusebox.AttachmentsPopupToggled",
                            shown);
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordAttachmentsMenuToggled(shown);
  }
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
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordFilePickedCount(
        contextual_search::ContextualSearchAttachmentButtonType::kTabPicker,
        count);
  }
}

- (void)recordAttachCountAtSubmission:(NSUInteger)count
                              forType:(ComposeboxMetricsAttachmentType)type {
  std::string histogram_name =
      "Omnibox.MobileFusebox.AttachmentCountAtSubmission.";
  histogram_name += GetStringForInputItemType(type);
  base::UmaHistogramCounts100(histogram_name, count);
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordAttachmentCountAtSubmission(
        MimeTypeFromMetricsAttachmentType(type), count);
  }
}

- (void)recordImagesAttached:(NSUInteger)count {
  base::UmaHistogramCounts100("Omnibox.MobileFusebox.ImagesAttached", count);
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordFilePickedCount(
        contextual_search::ContextualSearchAttachmentButtonType::kGallery,
        count);
  }
}

- (void)recordFilesAttached:(NSUInteger)count {
  base::UmaHistogramCounts100("Omnibox.MobileFusebox.FilesAttached", count);
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordFilePickedCount(
        contextual_search::ContextualSearchAttachmentButtonType::kFiles, count);
  }
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

- (void)recordAttachmentsMenuOpenedWithVisibleButtons:
    (std::vector<FuseboxAttachmentButtonType>)visibleButtons {
  [self recordAttachmentsMenuShown:YES];
  for (FuseboxAttachmentButtonType buttonType : visibleButtons) {
    [self recordAttachmentButtonShown:buttonType];
  }
}

- (void)recordTextEditedBeforeAiMode:(BOOL)edited {
  base::UmaHistogramBoolean("Omnibox.MobileFusebox.TextEditedBeforeAiMode",
                            edited);
}

- (void)recordToolSelected:(ComposeboxMode)tool {
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordToolSelected(
        ToolModeFromComposeboxMode(tool));
  }
}

- (void)recordModelSelected:(ComposeboxModelOption)model {
  if (_contextualSearchMetricsRecorder) {
    _contextualSearchMetricsRecorder->RecordModelSelected(
        ModelModeFromComposeboxModelOption(model));
  }
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
