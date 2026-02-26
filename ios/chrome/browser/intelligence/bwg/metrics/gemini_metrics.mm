// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace {
// Minimum time between FRE entry point impression logs.
const base::TimeDelta kGeminiImpressionThrottleInterval = base::Minutes(10);

// Returns the last impression time, persisting within the app session.
base::TimeTicks& GetLastGeminiImpressionTime() {
  static base::TimeTicks last_impression_time;
  return last_impression_time;
}

// Returns an IOSGeminiAspectRatioBucket for a given aspect ratio.
IOSGeminiAspectRatioBucket GetAspectRatioBucket(double aspect_ratio) {
  if (aspect_ratio <= 0) {
    return IOSGeminiAspectRatioBucket::kUnknown;
  }
  if (aspect_ratio < 0.3) {
    return IOSGeminiAspectRatioBucket::kVeryTall;
  }
  if (aspect_ratio < 0.8) {
    return IOSGeminiAspectRatioBucket::kTall;
  }
  if (aspect_ratio < 1.0) {
    return IOSGeminiAspectRatioBucket::kSlightlyTall;
  }
  if (aspect_ratio == 1.0) {
    return IOSGeminiAspectRatioBucket::kPerfectSquare;
  }
  if (aspect_ratio <= 1.2) {
    return IOSGeminiAspectRatioBucket::kSlightlyWide;
  }
  if (aspect_ratio <= 1.7) {
    return IOSGeminiAspectRatioBucket::kWide;
  }
  return IOSGeminiAspectRatioBucket::kVeryWide;
}

}  // namespace

namespace gemini {
IneligibilityReasons& IneligibilityReasons::set_workspace(bool value) {
  workspace = value;
  return *this;
}

IneligibilityReasons& IneligibilityReasons::set_chrome_enterprise(bool value) {
  chrome_enterprise = value;
  return *this;
}

IneligibilityReasons& IneligibilityReasons::set_account_capability(bool value) {
  account_capability = value;
  return *this;
}

IneligibilityReasons& IneligibilityReasons::set_authentication(bool value) {
  authentication = value;
  return *this;
}
}  // namespace gemini

const char kEligibilityHistogram[] = "IOS.Gemini.Eligibility";

const char kEntryPointHistogram[] = "IOS.Gemini.EntryPoint";

const char kFeedbackHistogram[] = "IOS.Gemini.Feedback";

const char kImageActionButtonHistogram[] = "IOS.Gemini.ImageActionButton";

const char kInputPlateAttachmentOptionHistogram[] =
    "IOS.Gemini.InputPlateAttachmentOption";

const char kFREEntryPointHistogram[] = "IOS.Gemini.FRE.EntryPoint";

const char kPromoActionHistogram[] = "IOS.Gemini.FRE.PromoAction";

const char kConsentActionHistogram[] = "IOS.Gemini.FRE.ConsentAction";

const char kGeminiIneligibilityReasonHistogram[] =
    "IOS.Gemini.IneligibilityReason";

const char kStartupTimeWithFREHistogram[] = "IOS.Gemini.StartupTime.FirstRun";

const char kStartupTimeNoFREHistogram[] = "IOS.Gemini.StartupTime.NotFirstRun";

const char kGeminiSessionCancellationHistogram[] =
    "IOS.Gemini.Session.CancellationReason";

const char kGeminiSessionLengthWithPromptHistogram[] =
    "IOS.Gemini.SessionLength.WithPrompt";

const char kGeminiSessionLengthAbandonedHistogram[] =
    "IOS.Gemini.SessionLength.Abandoned";

const char kGeminiSessionLengthFREWithPromptHistogram[] =
    "IOS.Gemini.SessionLength.FRE.WithPrompt";

const char kGeminiSessionLengthFREAbandonedHistogram[] =
    "IOS.Gemini.SessionLength.FRE.Abandoned";

const char kGeminiSessionTimeHistogram[] = "IOS.Gemini.Session.Time";

const char kFirstPromptSubmissionMethodHistogram[] =
    "IOS.Gemini.FirstPrompt.SubmissionMethod";

const char kPromptImagesAttachedCountHistogram[] =
    "IOS.Gemini.Prompt.ImagesAttached.Count";

const char kPromptImageRemixEnabledHistogram[] =
    "IOS.Gemini.Prompt.ImageRemix.Enabled";

const char kPromptLongPressImageIncludedHistogram[] =
    "IOS.Gemini.Prompt.LongPressImage.Included";

const char kPromptContextAttachmentHistogram[] =
    "IOS.Gemini.Prompt.ContextAttachment";

const char kResponseGeneratedImageIncluded[] =
    "IOS.Gemini.Response.GeneratedImage.Included";

const char kResponseLatencyWithContextHistogram[] =
    "IOS.Gemini.Response.Latency.WithContext";

const char kResponseLatencyWithoutContextHistogram[] =
    "IOS.Gemini.Response.Latency.WithoutContext";

const char kResponseLatencyWithGeneratedImageHistogram[] =
    "IOS.Gemini.Response.Latency.WithGeneratedImage";

const char kResponseLatencyWithoutGeneratedImageHistogram[] =
    "IOS.Gemini.Response.Latency.WithoutGeneratedImage";

const char kSessionPromptCountHistogram[] = "IOS.Gemini.Session.PromptCount";

const char kSessionFirstPromptHistogram[] = "IOS.Gemini.Session.FirstPrompt";

const char kFloatyTimeMinimizedHistogram[] = "IOS.Gemini.Floaty.TimeMinimized";

const char kFloatyViewStateTransitionHistogram[] =
    "IOS.Gemini.Floaty.ViewStateTransition";

const char kFloatyShownFromSourceHistogram[] =
    "IOS.Gemini.Floaty.ShownFromSource";

const char kFloatyHiddenFromSourceHistogram[] =
    "IOS.Gemini.Floaty.HiddenFromSource";

const char kFloatyDismissedStateHistogram[] =
    "IOS.Gemini.Floaty.DismissedState";

const char kImageRemixContextMenuEntryPointAspectRatioTappedHistogram[] =
    "IOS.Gemini.ImageRemix.ContextMenuEntryPoint.AspectRatio.Tapped";

const char kCameraFlowOSCameraAuthorizationInitialStatusHistogram[] =
    "IOS.Gemini.CameraFlow.OSCameraAuthorization.InitialStatus";

const char kCameraFlowOSAuthorizationRequestResultHistogram[] =
    "IOS.Gemini.CameraFlow.OSCameraAuthorizationRequest.Result";

const char kCameraFlowGoToOSSettingsAlertResultHistogram[] =
    "IOS.Gemini.CameraFlow.GoToOSSettingsAlert.Result";

const char kCameraFlowGeminiCameraPermissionInitialValueHistogram[] =
    "IOS.Gemini.CameraFlow.GeminiCameraPermission.InitialValue";

const char kCameraFlowGeminiCameraPermissionAlertResultHistogram[] =
    "IOS.Gemini.CameraFlow.GeminiCameraPermissionAlert.Result";

const char kCameraFlowCameraPickerResultHistogram[] =
    "IOS.Gemini.CameraFlow.CameraPicker.Result";

void RecordFREPromoAction(IOSGeminiFREAction action) {
  switch (action) {
    case IOSGeminiFREAction::kAccept:
      RecordFREPromoAccept();
      break;
    case IOSGeminiFREAction::kDismiss:
      RecordFREPromoDismiss();
      break;
    default:
      break;
  }
  base::UmaHistogramEnumeration(kPromoActionHistogram, action);
}

void RecordFREConsentAction(IOSGeminiFREAction action) {
  switch (action) {
    case IOSGeminiFREAction::kAccept:
      RecordFREConsentAccept();
      break;
    case IOSGeminiFREAction::kDismiss:
      RecordFREConsentDismiss();
      break;
    case IOSGeminiFREAction::kLinkClick:
      RecordFREConsentLinkClick();
      break;
    default:
      break;
  }
  base::UmaHistogramEnumeration(kConsentActionHistogram, action);
}

void RecordGeminiEligibility(bool eligible) {
  base::UmaHistogramBoolean(kEligibilityHistogram, eligible);
}

void RecordGeminiIneligibilityReasons(gemini::IneligibilityReasons reasons) {
  if (reasons.workspace) {
    base::UmaHistogramEnumeration(
        kGeminiIneligibilityReasonHistogram,
        IOSGeminiIneligibilityReason::kWorkspaceRestricted);
  }
  if (reasons.chrome_enterprise) {
    base::UmaHistogramEnumeration(
        kGeminiIneligibilityReasonHistogram,
        IOSGeminiIneligibilityReason::kChromeEnterpriseDisabled);
  }
  if (reasons.account_capability) {
    base::UmaHistogramEnumeration(
        kGeminiIneligibilityReasonHistogram,
        IOSGeminiIneligibilityReason::kInsufficientAccountCapability);
  }
  if (reasons.authentication) {
    base::UmaHistogramEnumeration(
        kGeminiIneligibilityReasonHistogram,
        IOSGeminiIneligibilityReason::kAccountUnauthenticated);
  }
}

void RecordGeminiSessionCancellation(
    IOSGeminiSessionCancellationReason reason) {
  base::UmaHistogramEnumeration(kGeminiSessionCancellationHistogram, reason);
}

void RecordGeminiSessionTime(base::TimeDelta session_duration) {
  base::UmaHistogramLongTimes100(kGeminiSessionTimeHistogram, session_duration);
}

void RecordGeminiSessionLengthByType(base::TimeDelta session_duration,
                                     bool is_first_run,
                                     IOSGeminiSessionType session_type) {
  if (is_first_run) {
    switch (session_type) {
      case IOSGeminiSessionType::kWithPrompt:
        base::UmaHistogramLongTimes100(
            kGeminiSessionLengthFREWithPromptHistogram, session_duration);
        break;
      case IOSGeminiSessionType::kAbandoned:
        base::UmaHistogramLongTimes100(
            kGeminiSessionLengthFREAbandonedHistogram, session_duration);
        break;
      case IOSGeminiSessionType::kUnknown:
        break;
    }
  } else {
    switch (session_type) {
      case IOSGeminiSessionType::kWithPrompt:
        base::UmaHistogramLongTimes100(kGeminiSessionLengthWithPromptHistogram,
                                       session_duration);
        break;
      case IOSGeminiSessionType::kAbandoned:
        base::UmaHistogramLongTimes100(kGeminiSessionLengthAbandonedHistogram,
                                       session_duration);
        break;
      case IOSGeminiSessionType::kUnknown:
        break;
    }
  }
}

void RecordGeminiEntryPointImpression() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks& last_impression_time = GetLastGeminiImpressionTime();

  // Check if enough time has passed since last impression.
  if (last_impression_time.is_null() ||
      (now - last_impression_time) >= kGeminiImpressionThrottleInterval) {
    base::RecordAction(
        base::UserMetricsAction("MobileGeminiEntryPointImpression"));
    last_impression_time = now;
  }
}

void RecordFREShown() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiFREShown"));
}

void RecordFirstResponseReceived() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiFirstResponseReceived"));
}

void RecordFirstPromptSubmission(IOSGeminiFirstPromptSubmissionMethod method) {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiFirstPromptSubmitted"));
  base::UmaHistogramEnumeration(kFirstPromptSubmissionMethodHistogram, method);
}

void RecordGeminiResponseReceived(bool generated_image_included) {
  base::RecordAction(base::UserMetricsAction("MobileGeminiResponseReceived"));
  base::UmaHistogramBoolean(kResponseGeneratedImageIncluded,
                            generated_image_included);
}

void RecordFREPromoAccept() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiFREPromoAccept"));
}

void RecordFREPromoDismiss() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiFREPromoCancel"));
}

void RecordFREConsentAccept() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiFREConsentAccept"));
}

void RecordFREConsentDismiss() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiFREConsentDismiss"));
}

void RecordFREConsentLinkClick() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiFREConsentLinkClick"));
}

void RecordResponseLatency(base::TimeDelta latency,
                           bool had_page_context,
                           bool had_generated_image) {
  if (had_page_context) {
    base::UmaHistogramMediumTimes(kResponseLatencyWithContextHistogram,
                                  latency);
  } else {
    base::UmaHistogramMediumTimes(kResponseLatencyWithoutContextHistogram,
                                  latency);
  }

  if (had_generated_image) {
    base::UmaHistogramMediumTimes(kResponseLatencyWithGeneratedImageHistogram,
                                  latency);
  } else {
    base::UmaHistogramMediumTimes(
        kResponseLatencyWithoutGeneratedImageHistogram, latency);
  }
}

void RecordSessionPromptCount(int prompt_count) {
  base::UmaHistogramCounts100(kSessionPromptCountHistogram, prompt_count);
}

void RecordSessionFirstPrompt(bool had_first_prompt) {
  base::UmaHistogramBoolean(kSessionFirstPromptHistogram, had_first_prompt);
}

void RecordFloatyExpandedToCollapsed() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiFloatyExpandedToCollapsed"));
  RecordGeminiViewStateTransition(
      IOSGeminiViewStateTransition::kExpandedToCollapsed);
}

void RecordFloatyCollapsedToExpanded() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiFloatyCollapsedToExpanded"));
  RecordGeminiViewStateTransition(
      IOSGeminiViewStateTransition::kCollapsedToExpanded);
}

void RecordFloatyDismissedState(ios::provider::GeminiViewState state) {
  base::UmaHistogramEnumeration(kFloatyDismissedStateHistogram, state);

  if (state == ios::provider::GeminiViewState::kCollapsed) {
    base::RecordAction(
        base::UserMetricsAction("MobileGeminiFloatyCollapsedToDismissed"));
  } else if (state == ios::provider::GeminiViewState::kExpanded) {
    base::RecordAction(
        base::UserMetricsAction("MobileGeminiFloatyExpandedToDismissed"));
  }
}

void RecordFloatyMinimizedTime(base::TimeTicks elapsed_minimized_floaty_time) {
  if (elapsed_minimized_floaty_time.is_null()) {
    return;
  }

  base::TimeDelta minimized_floaty_time =
      base::TimeTicks::Now() - elapsed_minimized_floaty_time;
  base::UmaHistogramLongTimes100(kFloatyTimeMinimizedHistogram,
                                 minimized_floaty_time);
}

void RecordGeminiViewStateTransition(IOSGeminiViewStateTransition transition) {
  base::UmaHistogramEnumeration(kFloatyViewStateTransitionHistogram,
                                transition);
}

void RecordGeminiViewStateHiddenToShown(
    ios::provider::GeminiViewState view_state) {
  switch (view_state) {
    case ios::provider::GeminiViewState::kCollapsed:
      RecordGeminiViewStateTransition(
          IOSGeminiViewStateTransition::kHiddenToCollapsed);
      break;
    case ios::provider::GeminiViewState::kExpanded:
      RecordGeminiViewStateTransition(
          IOSGeminiViewStateTransition::kHiddenToExpanded);
      break;
    default:
      break;
  }
}

void RecordFloatyShownFromSource(gemini::FloatyUpdateSource source) {
  base::UmaHistogramEnumeration(kFloatyShownFromSourceHistogram, source);
}

void RecordFloatyHiddenFromSource(gemini::FloatyUpdateSource source) {
  base::UmaHistogramEnumeration(kFloatyHiddenFromSourceHistogram, source);
}

void RecordURLOpened() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiURLOpened"));
}

void RecordGeminiEntryPointClick(gemini::EntryPoint entry_point,
                                 bool is_fre_flow) {
  if (entry_point == gemini::EntryPoint::Promo) {
    base::RecordAction(
        base::UserMetricsAction("MobileGeminiEntryPointAutomatic"));
  } else {
    base::RecordAction(base::UserMetricsAction("MobileGeminiEntryPointTapped"));
  }
  base::UmaHistogramEnumeration(kEntryPointHistogram, entry_point);
  if (is_fre_flow) {
    base::UmaHistogramEnumeration(kFREEntryPointHistogram, entry_point);
  }
}

void RecordGeminiNewChatButtonTapped() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiNewChatTapped"));
}

void RecordAIHubNewBadgeTapped() {
  base::RecordAction(base::UserMetricsAction("MobileAIHubNewBadgeTapped"));
}

void RecordAIHubIconTapped() {
  base::RecordAction(base::UserMetricsAction("MobileAIHubIconTapped"));
}

void RecordGeminiPromptSent(bool is_nano_banana_enabled,
                            int images_attached_count,
                            bool long_press_image_included,
                            bool has_page_context) {
  base::RecordAction(base::UserMetricsAction("MobileGeminiPromptSent"));
  base::UmaHistogramBoolean(kPromptImageRemixEnabledHistogram,
                            is_nano_banana_enabled);
  base::UmaHistogramCounts100(kPromptImagesAttachedCountHistogram,
                              images_attached_count);
  base::UmaHistogramBoolean(kPromptLongPressImageIncludedHistogram,
                            long_press_image_included);
  base::UmaHistogramBoolean(kPromptContextAttachmentHistogram,
                            has_page_context);
}

void RecordGeminiFeedback(IOSGeminiFeedback feedback) {
  base::UmaHistogramEnumeration(kFeedbackHistogram, feedback);

  switch (feedback) {
    case IOSGeminiFeedback::kThumbsUp:
      base::RecordAction(
          base::UserMetricsAction("MobileGeminiFeedbackThumbsUp"));
      break;
    case IOSGeminiFeedback::kThumbsDown:
      base::RecordAction(
          base::UserMetricsAction("MobileGeminiFeedbackThumbsDown"));
      break;
  }
}

void RecordImageRemixContextMenuEntryPointShown() {
  base::RecordAction(base::UserMetricsAction(
      "MobileGeminiImageRemixContextMenuEntryPointShown"));
}

void RecordImageRemixContextMenuEntryPointTapped(double aspect_ratio) {
  base::RecordAction(base::UserMetricsAction(
      "MobileGeminiImageRemixContextMenuEntryPointTapped"));
  base::UmaHistogramEnumeration(
      kImageRemixContextMenuEntryPointAspectRatioTappedHistogram,
      GetAspectRatioBucket(aspect_ratio));
}

void RecordGeminiCameraFlowBegan() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiCameraFlowBegan"));
}

void RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
    IOSGeminiOSCameraAuthorizationInitialStatus authorization_status) {
  base::UmaHistogramEnumeration(
      kCameraFlowOSCameraAuthorizationInitialStatusHistogram,
      authorization_status);
}

void RecordGeminiCameraFlowOSAuthorizationResult(bool granted) {
  if (granted) {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowOSCameraAuthorizationRequestGranted"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowOSCameraAuthorizationRequestDenied"));
  }
  base::UmaHistogramEnumeration(
      kCameraFlowOSAuthorizationRequestResultHistogram,
      granted ? IOSGeminiCameraFlowOSCameraAuthorizationResult::kGranted
              : IOSGeminiCameraFlowOSCameraAuthorizationResult::kDenied);
}

void RecordGeminiCameraFlowGoToOSSettingsAlertResult(bool accepted) {
  if (accepted) {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowGoToOSSettingsAlertGoToSettings"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowGoToOSSettingsAlertNoThanks"));
  }
  base::UmaHistogramEnumeration(
      kCameraFlowGoToOSSettingsAlertResultHistogram,
      accepted ? IOSGeminiGoToOSSettingsAlertResult::kGoToSettings
               : IOSGeminiGoToOSSettingsAlertResult::kNoThanks);
}

void RecordGeminiCameraFlowGeminiCameraPermissionInitialValue(bool enabled) {
  base::UmaHistogramBoolean(
      kCameraFlowGeminiCameraPermissionInitialValueHistogram, enabled);
}

void RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(bool allowed) {
  if (allowed) {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowGeminiCameraPermissionAlertAllow"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileGeminiCameraFlowGeminiCameraPermissionAlertDontAllow"));
  }
  base::UmaHistogramEnumeration(
      kCameraFlowGeminiCameraPermissionAlertResultHistogram,
      allowed ? IOSGeminiCameraPermissionAlertResult::kAllow
              : IOSGeminiCameraPermissionAlertResult::kDontAllow);
}

void RecordGeminiCameraFlowPresentCameraPicker() {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiCameraFlowCameraPickerPresented"));
}

void RecordGeminiCameraFlowCameraPickerResult(
    IOSGeminiCameraPickerResult result) {
  switch (result) {
    case IOSGeminiCameraPickerResult::kCancelled:
      base::RecordAction(base::UserMetricsAction(
          "MobileGeminiCameraFlowCameraPickerCancelled"));
      break;
    case IOSGeminiCameraPickerResult::kFinishedWithoutImage:
      base::RecordAction(base::UserMetricsAction(
          "MobileGeminiCameraFlowCameraPickerFinishedWithoutImage"));
      break;
    case IOSGeminiCameraPickerResult::kFinishedWithImage:
      base::RecordAction(base::UserMetricsAction(
          "MobileGeminiCameraFlowCameraPickerFinishedWithImage"));
      break;
  }
  base::UmaHistogramEnumeration(kCameraFlowCameraPickerResultHistogram, result);
}

void RecordGeminiImageActionButtonTapped(gemini::ImageActionButtonType type) {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiImageActionButtonTapped"));
  base::UmaHistogramEnumeration(kImageActionButtonHistogram, type);
}

void RecordGeminiInputPlateAttachmentOptionTapped(
    gemini::InputPlateAttachmentOption option) {
  base::RecordAction(
      base::UserMetricsAction("MobileGeminiInputPlateAttachmentOptionTapped"));
  base::UmaHistogramEnumeration(kInputPlateAttachmentOptionHistogram, option);
}
