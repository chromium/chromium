// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_METRICS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, GeminiInputType);

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace gemini {
enum class EntryPoint;
enum class FloatyUpdateSource;
enum class ImageActionButtonType;
enum class InputPlateAttachmentOption;
// Encapsulates a set of ineligibility reasons computed during a single Gemini
// eligibility check.
struct IneligibilityReasons {
  bool workspace = false;
  bool chrome_enterprise = false;
  bool account_capability = false;
  bool authentication = false;

  IneligibilityReasons() = default;

  IneligibilityReasons& set_workspace(bool value);
  IneligibilityReasons& set_chrome_enterprise(bool value);
  IneligibilityReasons& set_account_capability(bool value);
  IneligibilityReasons& set_authentication(bool value);
};
}  // namespace gemini

namespace ios::provider {
enum class GeminiViewState;
}

// UMA histogram key for IOS.Gemini.Eligibility.
extern const char kEligibilityHistogram[];

// UMA histogram key for IOS.Gemini.EntryPoint.
extern const char kEntryPointHistogram[];

// UMA histogram key for IOS.Gemini.FRE.EntryPoint.
extern const char kFREEntryPointHistogram[];

// UMA histogram key for IOS.Gemini.FRE.PromoAction.
extern const char kPromoActionHistogram[];

// UMA histogram key for IOS.Gemini.FRE.ConsentAction.
extern const char kConsentActionHistogram[];

// UMA histogram key for IOS.Gemini.Session.PromptCount.
extern const char kSessionPromptCountHistogram[];

// UMA histogram key for IOS.Gemini.Session.FirstPrompt.
extern const char kSessionFirstPromptHistogram[];

// UMA histogram key for IOS.Gemini.Floaty.TimeMinimized.
extern const char kFloatyTimeMinimizedHistogram[];

// UMA histogram key for IOS.Gemini.Floaty.ViewStateTransition.
extern const char kFloatyViewStateTransitionHistogram[];

// UMA histogram key for IOS.Gemini.Floaty.ShownFromSource.
extern const char kFloatyShownFromSourceHistogram[];

// UMA histogram key for IOS.Gemini.Floaty.HiddenFromSource.
extern const char kFloatyHiddenFromSourceHistogram[];

// UMA histogram key for IOS.Gemini.Floaty.DismissedState.
extern const char kFloatyDismissedStateHistogram[];

// Enum for the IOS.Gemini.FRE.PromoAction and IOS.Gemini.FRE.ConsentAction
// histograms.
// LINT.IfChange(IOSGeminiFREAction)
enum class IOSGeminiFREAction {
  kAccept = 0,
  kDismiss = 1,
  kLinkClick = 2,
  kMaxValue = kLinkClick,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFREAction)

// Records the user action on the FRE Promo.
void RecordFREPromoAction(IOSGeminiFREAction action);

// Records the user action on the FRE Consent Screen.
void RecordFREConsentAction(IOSGeminiFREAction action);

// Enum for tracking Gemini ineligibility reasons.
// LINT.IfChange(IOSGeminiIneligibilityReason)
enum class IOSGeminiIneligibilityReason {
  kWorkspaceRestricted = 0,
  kChromeEnterpriseDisabled = 1,
  kInsufficientAccountCapability = 2,
  kAccountUnauthenticated = 3,
  kMaxValue = kAccountUnauthenticated
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiIneligibilityReason)

// UMA histogram key for IOS.Gemini.IneligibilityReason.
extern const char kGeminiIneligibilityReasonHistogram[];

// UMA histogram key for IOS.Gemini.StartupTime.FirstRun.
extern const char kStartupTimeWithFREHistogram[];

// UMA histogram key for IOS.Gemini.StartupTime.NotFirstRun.
extern const char kStartupTimeNoFREHistogram[];

// Enum for tracking session cancellation reasons.
// LINT.IfChange(IOSGeminiSessionCancellationReason)
enum class IOSGeminiSessionCancellationReason {
  kUnknown = 0,
  kStopButtonTapped = 1,
  kOutsideTapped = 2,
  kExpandedStateCloseButtonTapped = 3,
  kCollapsedStateCloseButtonTapped = 4,
  kLoadingStateCloseButtonTapped = 5,
  kMaxValue = kLoadingStateCloseButtonTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiSessionCancellationReason)

// Records the reason for a gemini session cancellation.
void RecordGeminiSessionCancellation(IOSGeminiSessionCancellationReason reason);

// UMA histogram key for IOS.Gemini.Session.CancellationReason.
extern const char kGeminiSessionCancellationHistogram[];

// UMA histogram key for IOS.Gemini.Session.Time.
extern const char kGeminiSessionTimeHistogram[];

// UMA histogram key for IOS.Gemini.SessionLength.WithPrompt.
extern const char kGeminiSessionLengthWithPromptHistogram[];

// UMA histogram key for IOS.Gemini.SessionLength.Abandoned.
extern const char kGeminiSessionLengthAbandonedHistogram[];

// UMA histogram key for IOS.Gemini.SessionLength.FRE.WithPrompt.
extern const char kGeminiSessionLengthFREWithPromptHistogram[];

// UMA histogram key for IOS.Gemini.SessionLength.FRE.Abandoned.
extern const char kGeminiSessionLengthFREWithAbandonedHistogram[];

// TODO(crbug.com/481711842): Replace this enum and its
// gemini_session_delegate.h equivalent with an enum in gemini_constants.h
// Enum for the IOS.Gemini.FirstPrompt.SubmissionMethod histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiFirstPromptSubmissionMethod)
enum class IOSGeminiFirstPromptSubmissionMethod {
  kText = 0,
  kSummarize = 1,
  kCheckThisSite = 2,
  kFindRelatedSites = 3,
  kAskAboutPage = 4,
  kCreateFaq = 5,
  kUnknown = 6,
  kZeroStateSuggestions = 7,
  kWhatCanGeminiDo = 8,
  kDiscoveryCard = 9,
  kOmniboxSummarize = 10,
  kOmniboxPrompt = 11,
  kTransitionToLive = 12,
  kOnboardingWhatCanGeminiDo = 13,
  kOnboardingAskAboutPage = 14,
  kOnboardingSummarize = 15,
  kSuggestedReply = 16,
  kNanoBananaTurnThisPageIntoAComicStrip = 17,
  kNanoBananaMakeAFolkArtIllustration = 18,
  kNanoBananaMakeACustomMiniFigure = 19,
  kNanoBananaGiveMeAGrungeMakeover = 20,
  kNanoBananaTurnThisImageIntoAVintagePostcard = 21,
  kNanoBananaTurnThisImageIntoAWatercolorPainting = 22,
  kNanoBananaMakeThisImageLookLikeInstantFilm = 23,
  kMaxValue = kNanoBananaMakeThisImageLookLikeInstantFilm,
};
// LINT.ThenChange(
//   /tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFirstPromptSubmissionMethod,
//   /ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h:BWGInputType
// )

// UMA histogram key for IOS.Gemini.FirstPrompt.SubmissionMethod.
extern const char kFirstPromptSubmissionMethodHistogram[];

// UMA histogram key for IOS.Gemini.Prompt.ImagesAttached.Count.
extern const char kPromptImagesAttachedCountHistogram[];

// UMA histogram key for IOS.Gemini.Prompt.ImageRemix.Enabled.
extern const char kPromptImageRemixEnabledHistogram[];

// UMA histogram key for IOS.Gemini.Prompt.LongPressImage.Included.
extern const char kPromptLongPressImageIncludedHistogram[];

// UMA histogram key for IOS.Gemini.Prompt.ContextAttachment.
extern const char kPromptContextAttachmentHistogram[];

// UMA histogram key for IOS.Gemini.Response.GeneratedImage.Included.
extern const char kResponseGeneratedImageIncluded[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithContext.
extern const char kResponseLatencyWithContextHistogram[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithoutContext.
extern const char kResponseLatencyWithoutContextHistogram[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithGeneratedImage.
extern const char kResponseLatencyWithGeneratedImageHistogram[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithoutGeneratedImage.
extern const char kResponseLatencyWithoutGeneratedImageHistogram[];

// Represents the completed Gemini session types.
enum class IOSGeminiSessionType {
  kUnknown = 0,
  kWithPrompt = 1,
  kAbandoned = 2,
  kMaxValue = kAbandoned,
};

// TODO(crbug.com/481711842): Replace this enum and its
// gemini_session_delegate.h equivalent with an enum in gemini_constants.h
// Enum for the IOS.Gemini.Feedback histogram.
// LINT.IfChange(IOSGeminiFeedback)
enum class IOSGeminiFeedback {
  kThumbsUp = 0,
  kThumbsDown = 1,
  kMaxValue = kThumbsDown,
};
// LINT.ThenChange(
//    /ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h:GeminiFeedbackType,
//    /tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFeedback
//)

// UMA histogram key for IOS.Gemini.Feedback.
extern const char kFeedbackHistogram[];

// Enum for the IOS.Gemini.ImageRemix.ContextMenuEntryPoint.AspectRatio.*
// histograms.
// LINT.IfChange(IOSGeminiAspectRatioBucket)
enum class IOSGeminiAspectRatioBucket {
  kUnknown = 0,
  kVeryTall = 1,
  kTall = 2,
  kSlightlyTall = 3,
  kPerfectSquare = 4,
  kSlightlyWide = 5,
  kWide = 6,
  kVeryWide = 7,
  kMaxValue = kVeryWide,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiAspectRatioBucket)

// Enum for the IOS.Gemini.CameraFlow.OSCameraAuthorization.InitialStatus
// histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiOSCameraAuthorizationInitialStatus)
enum class IOSGeminiOSCameraAuthorizationInitialStatus {
  kNotDetermined = 0,
  kRestricted = 1,
  kDenied = 2,
  kAuthorized = 3,
  kSourceTypeUnavailable = 4,
  kMaxValue = kSourceTypeUnavailable,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiOSCameraAuthorizationInitialStatus)

// UMA histogram key for
// IOS.Gemini.CameraFlow.OSCameraAuthorization.InitialStatus.
extern const char kCameraFlowOSCameraAuthorizationInitialStatusHistogram[];

// Enum for the IOS.Gemini.CameraFlow.OSCameraAuthorization.Result histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiCameraFlowOSCameraAuthorizationResult)
enum class IOSGeminiCameraFlowOSCameraAuthorizationResult {
  kGranted = 0,
  kDenied = 1,
  kMaxValue = kDenied,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiCameraFlowOSCameraAuthorizationResult)

// UMA histogram key for
// IOS.Gemini.CameraFlow.OSCameraAuthorizationRequest.Result.
extern const char kCameraFlowOSAuthorizationRequestResultHistogram[];

// Enum for the IOS.Gemini.CameraFlow.GoToOSSettingsAlert.Result histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiGoToOSSettingsAlertResult)
enum class IOSGeminiGoToOSSettingsAlertResult {
  kGoToSettings = 0,
  kNoThanks = 1,
  kMaxValue = kNoThanks,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiGoToOSSettingsAlertResult)

// UMA histogram key for
// IOS.Gemini.CameraFlow.GoToOSSettingsAlert.Result.
extern const char kCameraFlowGoToOSSettingsAlertResultHistogram[];

// UMA histogram key for
// IOS.Gemini.CameraFlow.GeminiCameraPermission.InitialValue.
extern const char kCameraFlowGeminiCameraPermissionInitialValueHistogram[];

// Enum for the IOS.Gemini.CameraFlow.GeminiCameraPermissionAlert.Result
// histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiCameraPermissionAlertResult)
enum class IOSGeminiCameraPermissionAlertResult {
  kAllow = 0,
  kDontAllow = 1,
  kMaxValue = kDontAllow,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiCameraPermissionAlertResult)

// UMA histogram key for
// IOS.Gemini.CameraFlow.GeminiCameraPermissionAlert.Result.
extern const char kCameraFlowGeminiCameraPermissionAlertResultHistogram[];

// Enum for the IOS.Gemini.CameraFlow.CameraPicker.Result histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSGeminiCameraPickerResult)
enum class IOSGeminiCameraPickerResult {
  kCancelled = 0,
  kFinishedWithoutImage = 1,
  kFinishedWithImage = 2,
  kMaxValue = kFinishedWithImage,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiCameraPickerResult)

// UMA histogram key for
// IOS.Gemini.CameraFlow.CameraPicker.Result.
extern const char kCameraFlowCameraPickerResultHistogram[];

// UMA histogram key for
// IOS.Gemini.ImageRemix.ContextMenuEntryPoint.AspectRatio.Tapped.
extern const char kImageRemixContextMenuEntryPointAspectRatioTappedHistogram[];

// UMA histogram key for IOS.Gemini.ImageActionButton.
extern const char kImageActionButtonHistogram[];

// UMA histogram key for IOS.Gemini.InputPlateAttachmentOption.
extern const char kInputPlateAttachmentOptionHistogram[];

// Records that an image action button was tapped.
void RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType type);

// Records that an input plate attachment option was tapped.
void RecordGeminiInputPlateAttachmentOptionTapped(
    gemini::InputPlateAttachmentOption option);

// Records that the Image Remix context menu entry point was shown.
void RecordImageRemixContextMenuEntryPointShown();

// Records that the Image Remix context menu entry point was tapped with the
// given image aspect ratio.
void RecordImageRemixContextMenuEntryPointTapped(double aspect_ratio);

// Records user feedback on a Gemini response.
void RecordGeminiFeedback(IOSGeminiFeedback feedback);

// Records the duration of a Gemini session.
void RecordGeminiSessionTime(base::TimeDelta session_duration);

// Records the duration of a Gemini session by type of session.
void RecordGeminiSessionLengthByType(base::TimeDelta session_duration,
                                     bool is_first_run,
                                     IOSGeminiSessionType session_type);

// Records when user sees the Gemini entry point impression.
// Can be called once every 10 minutes to avoid spam logging.
void RecordGeminiEntryPointImpression();

// Records that the Gemini FRE was shown.
void RecordFREShown();

// Records user action for first response received.
void RecordFirstResponseReceived();

// Records that the user submitted their first prompt.
void RecordFirstPromptSubmission(IOSGeminiFirstPromptSubmissionMethod method);

// Records that the user received a response from Gemini with a boolean
// indicating whether a generated image was included in the response.
void RecordGeminiResponseReceived(bool generated_image_included);

// Records that the user tapped the "Get Started" button on the Gemini FRE promo
// screen.
void RecordFREPromoAccept();

// Records that the user tapped the "Cancel" button on the Gemini FRE promo
// screen.
void RecordFREPromoDismiss();

// Records that the user accepted the Gemini FRE consent.
void RecordFREConsentAccept();

// Records that the user dismissed the Gemini FRE consent.
void RecordFREConsentDismiss();

// Records that the user clicked a link on the Gemini FRE consent screen.
void RecordFREConsentLinkClick();

// Records prompt context attachment metrics.
void RecordPromptContextAttachment(bool has_page_context);

// Records the latency from prompt submission to response received, including
// metadata about the prompt & response.
void RecordResponseLatency(base::TimeDelta latency,
                           bool had_page_context,
                           bool had_generated_image);

// Records the total number of prompts sent in a Gemini session.
void RecordSessionPromptCount(int prompt_count);

// Records if a first prompt was sent in a Gemini session.
void RecordSessionFirstPrompt(bool had_first_prompt);

// Enum for the IOS.Gemini.ViewStateTransition histogram.
// LINT.IfChange(IOSGeminiViewStateTransition)
enum class IOSGeminiViewStateTransition {
  kUnknown = 0,
  kCollapsedToExpanded = 1,
  kExpandedToCollapsed = 2,
  kHiddenToCollapsed = 3,
  kHiddenToExpanded = 4,
  kMaxValue = kHiddenToExpanded,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiViewStateTransition)

// Records the floaty transition from expanded to collapsed.
void RecordFloatyExpandedToCollapsed();

// Records the floaty transition from collapsed to expanded.
void RecordFloatyCollapsedToExpanded();

// Records the floaty dismissing with the given state.
void RecordFloatyDismissedState(ios::provider::GeminiViewState state);

// Records the length of time a floaty is minimized until it is expanded.
void RecordFloatyMinimizedTime(base::TimeTicks elapsed_minimized_floaty_time);

// Records whether a Gemini eligibility check was successful.
void RecordGeminiEligibility(bool eligible);

// Records all of the Gemini ineligibility reasons. One record will be sent at
// most per associated value of IOSGeminiIneligibilityReason.
void RecordGeminiIneligibilityReasons(gemini::IneligibilityReasons reasons);

// Records the Gemini floaty view state transition.
void RecordGeminiViewStateTransition(IOSGeminiViewStateTransition transition);

// Records the `view_state` that will be shown from the hidden state.
void RecordGeminiViewStateHiddenToShown(
    ios::provider::GeminiViewState view_state);

// Records the floaty being shown from the `source` that triggered the call.
void RecordFloatyShownFromSource(gemini::FloatyUpdateSource source);

// Records the floaty being hidden from the `source` that triggered the call.
void RecordFloatyHiddenFromSource(gemini::FloatyUpdateSource source);

// Records that the user clicked a URL in a Gemini session.
void RecordURLOpened();

// Records entry point metrics with context about whether FRE is shown.
void RecordGeminiEntryPointClick(gemini::EntryPoint entry_point,
                                 bool is_fre_flow);

// Records that the user tapped the new chat button in a Gemini session.
void RecordGeminiNewChatButtonTapped();

// Records that the AI Hub new badge was tapped.
void RecordAIHubNewBadgeTapped();

// Records that the AI Hub icon was tapped.
void RecordAIHubIconTapped();

// Records that the user sent a prompt in a Gemini session. Includes parameters
// for histogram metrics.
void RecordGeminiPromptSent(bool is_nano_banana_enabled,
                            int images_attached_count,
                            bool long_press_image_included,
                            bool has_page_context);

// Records the result of an OS-level camera authorization request.
void RecordGeminiCameraFlowOSAuthorizationResult(bool granted);

// Records the result of the alert directing users to OS settings.
void RecordGeminiCameraFlowGoToOSSettingsAlertResult(bool accepted);

// Records the result of the Gemini camera permission alert.
void RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(bool accepted);

// Records that the Gemini camera flow began.
void RecordGeminiCameraFlowBegan();

// Records the initial OS camera authorization status value.
void RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
    IOSGeminiOSCameraAuthorizationInitialStatus authorization_status);

// Records the result of an OS-level camera authorization request.
void RecordGeminiCameraFlowOSAuthorizationResult(bool granted);

// Records the result of the alert directing users to OS settings.
void RecordGeminiCameraFlowGoToOSSettingsAlertResult(bool accepted);

// Records the initial Gemini camera permission value.
void RecordGeminiCameraFlowGeminiCameraPermissionInitialValue(bool enabled);

// Records the result of the Gemini camera permission alert.
void RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(bool accepted);

// Records that the camera picker was presented.
void RecordGeminiCameraFlowPresentCameraPicker();

// Records the result of the camera picker.
void RecordGeminiCameraFlowCameraPickerResult(
    IOSGeminiCameraPickerResult result);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_GEMINI_METRICS_H_
