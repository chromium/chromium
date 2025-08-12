// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, BWGInputType);

namespace base {
class TimeDelta;
}  // namespace base

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

// UMA histogram key for IOS.Gemini.StartupTime.FirstRun.
extern const char kStartupTimeWithFREHistogram[];

// UMA histogram key for IOS.Gemini.StartupTime.NotFirstRun.
extern const char kStartupTimeNoFREHistogram[];

// UMA histogram key for IOS.Gemini.Session.Time.
extern const char kBWGSessionTimeHistogram[];

// Enum for the IOS.Gemini.FirstPrompt.SubmissionMethod histogram.
// LINT.IfChange(IOSGeminiFirstPromptSubmissionMethod)
enum class IOSGeminiFirstPromptSubmissionMethod {
  kText = 0,
  kSummarize = 1,
  kCheckThisSite = 2,
  kFindRelatedSites = 3,
  kAskAboutPage = 4,
  kUnknown = 5,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSGeminiFirstPromptSubmissionMethod)

// UMA histogram key for IOS.Gemini.FirstPrompt.SubmissionMethod.
extern const char kFirstPromptSubmissionMethodHistogram[];

// UMA histogram key for IOS.Gemini.Prompt.ContextAttachment.
extern const char kPromptContextAttachmentHistogram[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithContext.
extern const char kResponseLatencyWithContextHistogram[];

// UMA histogram key for IOS.Gemini.Response.Latency.WithoutContext.
extern const char kResponseLatencyWithoutContextHistogram[];

// Records the duration of a Gemini session.
void RecordBWGSessionTime(base::TimeDelta session_duration);

// Records when user sees the Gemini entry point impression.
// Can be called once every 10 minutes to avoid spam logging.
void RecordGeminiEntryPointImpression();

// Records that the BWG FRE was shown.
void RecordFREShown();

// Records user action for first response received.
void RecordFirstResponseReceived();

// Records that the user submitted their first prompt and how it was submitted.
void RecordFirstPromptSubmission(IOSGeminiFirstPromptSubmissionMethod method);

// Records that the user received any response from BWG.
void RecordBWGResponseReceived();

// Records that the user tapped the "Get Started" button on the BWG FRE promo
// screen.
void RecordFREPromoAccept();

// Records that the user tapped the "Cancel" button on the BWG FRE promo screen.
void RecordFREPromoDismiss();

// Records that the user accepted the BWG FRE consent.
void RecordFREConsentAccept();

// Records that the user dismissed the BWG FRE consent.
void RecordFREConsentDismiss();

// Records that the user clicked a link on the BWG FRE consent screen.
void RecordFREConsentLinkClick();

// Records prompt context attachment metrics.
void RecordPromptContextAttachment(bool has_page_context);

// Records the latency from prompt submission to response received.
void RecordResponseLatency(base::TimeDelta latency, bool had_page_context);

// Records the total number of prompts sent in a BWG session.
void RecordSessionPromptCount(int prompt_count);

// Records if a first prompt was sent in a BWG session.
void RecordSessionFirstPrompt(bool had_first_prompt);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_METRICS_BWG_METRICS_H_
