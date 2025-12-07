// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"

namespace {
// Minimum time between FRE entry point impression logs.
const base::TimeDelta kGeminiImpressionThrottleInterval = base::Minutes(10);

// Returns the last impression time, persisting within the app session.
base::TimeTicks& GetLastGeminiImpressionTime() {
  static base::TimeTicks last_impression_time;
  return last_impression_time;
}
}  // namespace

const char kEligibilityHistogram[] = "IOS.Gemini.Eligibility";

const char kEntryPointHistogram[] = "IOS.Gemini.EntryPoint";

const char kFREEntryPointHistogram[] = "IOS.Gemini.FRE.EntryPoint";

const char kPromoActionHistogram[] = "IOS.Gemini.FRE.PromoAction";

const char kConsentActionHistogram[] = "IOS.Gemini.FRE.ConsentAction";

const char kStartupTimeWithFREHistogram[] = "IOS.Gemini.StartupTime.FirstRun";

const char kStartupTimeNoFREHistogram[] = "IOS.Gemini.StartupTime.NotFirstRun";

const char kBWGSessionLengthWithPromptHistogram[] =
    "IOS.Gemini.SessionLength.WithPrompt";

const char kBWGSessionLengthAbandonedHistogram[] =
    "IOS.Gemini.SessionLength.Abandoned";

const char kBWGSessionLengthFREWithPromptHistogram[] =
    "IOS.Gemini.SessionLength.FRE.WithPrompt";

const char kBWGSessionLengthFREAbandonedHistogram[] =
    "IOS.Gemini.SessionLength.FRE.Abandoned";

const char kBWGSessionTimeHistogram[] = "IOS.Gemini.Session.Time";

const char kFirstPromptSubmissionMethodHistogram[] =
    "IOS.Gemini.FirstPrompt.SubmissionMethod";

const char kPromptContextAttachmentHistogram[] =
    "IOS.Gemini.Prompt.ContextAttachment";

const char kResponseLatencyWithContextHistogram[] =
    "IOS.Gemini.Response.Latency.WithContext";

const char kResponseLatencyWithoutContextHistogram[] =
    "IOS.Gemini.Response.Latency.WithoutContext";

const char kSessionPromptCountHistogram[] = "IOS.Gemini.Session.PromptCount";

const char kSessionFirstPromptHistogram[] = "IOS.Gemini.Session.FirstPrompt";

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

void RecordBWGSessionTime(base::TimeDelta session_duration) {
  base::UmaHistogramLongTimes100(kBWGSessionTimeHistogram, session_duration);
}

void RecordBWGSessionLengthByType(base::TimeDelta session_duration,
                                  bool is_first_run,
                                  IOSGeminiSessionType session_type) {
  if (is_first_run) {
    switch (session_type) {
      case IOSGeminiSessionType::kWithPrompt:
        base::UmaHistogramLongTimes100(kBWGSessionLengthFREWithPromptHistogram,
                                       session_duration);
        break;
      case IOSGeminiSessionType::kAbandoned:
        base::UmaHistogramLongTimes100(kBWGSessionLengthFREAbandonedHistogram,
                                       session_duration);
        break;
      case IOSGeminiSessionType::kUnknown:
        break;
    }
  } else {
    switch (session_type) {
      case IOSGeminiSessionType::kWithPrompt:
        base::UmaHistogramLongTimes100(kBWGSessionLengthWithPromptHistogram,
                                       session_duration);
        break;
      case IOSGeminiSessionType::kAbandoned:
        base::UmaHistogramLongTimes100(kBWGSessionLengthAbandonedHistogram,
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

void RecordBWGResponseReceived() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiResponseReceived"));
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

void RecordPromptContextAttachment(bool has_page_context) {
  base::UmaHistogramBoolean(kPromptContextAttachmentHistogram,
                            has_page_context);
}

void RecordResponseLatency(base::TimeDelta latency, bool had_page_context) {
  if (had_page_context) {
    base::UmaHistogramMediumTimes(kResponseLatencyWithContextHistogram,
                                  latency);
  } else {
    base::UmaHistogramMediumTimes(kResponseLatencyWithoutContextHistogram,
                                  latency);
  }
}

void RecordSessionPromptCount(int prompt_count) {
  base::UmaHistogramCounts100(kSessionPromptCountHistogram, prompt_count);
}

void RecordSessionFirstPrompt(bool had_first_prompt) {
  base::UmaHistogramBoolean(kSessionFirstPromptHistogram, had_first_prompt);
}

void RecordURLOpened() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiURLOpened"));
}

void RecordBWGEntryPointClick(bwg::EntryPoint entry_point, bool is_fre_flow) {
  if (entry_point == bwg::EntryPoint::Promo) {
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

void RecordBWGNewChatButtonTapped() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiNewChatTapped"));
}

void RecordAIHubNewBadgeTapped() {
  base::RecordAction(base::UserMetricsAction("MobileAIHubNewBadgeTapped"));
}

void RecordAIHubIconTapped() {
  base::RecordAction(base::UserMetricsAction("MobileAIHubIconTapped"));
}

void RecordBWGPromptSent() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiPromptSent"));
}

void RecordBWGSettingsOpened() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsOpened"));
}

void RecordBWGSettingsClose() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsClose"));
}

void RecordBWGSettingsBack() {
  base::RecordAction(base::UserMetricsAction("MobileGeminiSettingsBack"));
}

void RecordBWGSettingsAppActivity() {
  base::RecordAction(
      base::UserMetricsAction("Settings.BWGSettings.BWGAppActivity"));
}

void RecordBWGSettingsExtensions() {
  base::RecordAction(
      base::UserMetricsAction("Settings.BWGSettings.BWGExtensions"));
}
