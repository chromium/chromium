// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_CONSTANTS_H_

#import <string_view>

// Histogram names for the Assistant AIM bottom sheet.
inline constexpr std::string_view kAssistantAimEntrypointHistogram =
    "IOS.SicBottomSheet.Entrypoint";
inline constexpr std::string_view kAssistantAimStateTransitionHistogram =
    "IOS.SicBottomSheet.StateTransition";
inline constexpr std::string_view kAssistantAimCloseUndoTappedHistogram =
    "IOS.SicBottomSheet.CloseUndoTapped";
inline constexpr std::string_view kAssistantAimSessionHadEngagementHistogram =
    "IOS.SicBottomSheet.SessionHadEngagement";
inline constexpr std::string_view kAssistantAimTimeSpentHistogram =
    "IOS.SicBottomSheet.TimeSpent";
inline constexpr std::string_view kAssistantAimTimeSpentEngagedHistogram =
    "IOS.SicBottomSheet.TimeSpent.Engaged";
inline constexpr std::string_view kAssistantAimTimeSpentAbandonedHistogram =
    "IOS.SicBottomSheet.TimeSpent.Abandoned";
inline constexpr std::string_view kAssistantAimImmediateDismissalHistogram =
    "IOS.SicBottomSheet.ImmediateDismissal";
inline constexpr std::string_view kAssistantAimHeaderActionHistogram =
    "IOS.SicBottomSheet.HeaderAction";
inline constexpr std::string_view kAssistantAimLinkClickedHistogram =
    "IOS.SicBottomSheet.LinkClicked";
inline constexpr std::string_view kAssistantAimLinksClickedPerSessionHistogram =
    "IOS.SicBottomSheet.LinksClickedPerSession";
inline constexpr std::string_view kAssistantAimSessionHadLinkClickHistogram =
    "IOS.SicBottomSheet.SessionHadLinkClick";
inline constexpr std::string_view kAssistantAimReturnedAfterLinkClickHistogram =
    "IOS.SicBottomSheet.ReturnedAfterLinkClick";
inline constexpr std::string_view kAssistantAimFollowupsPerSessionHistogram =
    "IOS.SicBottomSheet.FollowupsPerSession";
inline constexpr std::string_view kAssistantAimFollowupHadAttachmentHistogram =
    "IOS.SicBottomSheet.FollowupHadAttachment";
inline constexpr std::string_view kAssistantAimAttachmentsPerSessionHistogram =
    "IOS.SicBottomSheet.AttachmentsPerSession";
inline constexpr std::string_view kAssistantAimAutoAddedTabRemovedHistogram =
    "IOS.SicBottomSheet.AutoAddedTabRemoved";
inline constexpr std::string_view kAssistantAimFRENoticeActionHistogram =
    "IOS.SicBottomSheet.FRENoticeAction";

// Entrypoints for starting an Assistant AIM bottom sheet session.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AssistantAimEntrypoint)
enum class AssistantAimEntrypoint {
  kUnknown = 0,
  // User submitted an AIM query with the current page attached ("Ask about
  // current page").
  kAskAboutCurrentPage = 1,
  // User triggered "Search With" from the web selection edit menu.
  kWebSelectionSearchWith = 2,
  // User tapped an outbound link on an AIM SRP tab.
  kAimSrpLinkTap = 3,
  // User tapped the Assistant action in the Location Bar or Toolbar.
  kLocationBarAction = 4,
  // Active session restored automatically on tab or scene activation.
  kSessionRestore = 5,
  kMaxValue = kSessionRestore,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSicBottomSheetEntrypoint)

// State transitions of the Assistant AIM bottom sheet.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AssistantAimStateTransition)
enum class AssistantAimStateTransition {
  kMinimizedSheet = 0,
  kMediumSheet = 1,
  kLargeSheet = 2,
  kDismissedCloseButton = 3,
  kRestoredWithUndo = 4,
  kDismissedOther = 5,
  kMaxValue = kDismissedOther,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSicBottomSheetStateTransition)

// Actions available in the Assistant AIM bottom sheet header and overflow menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AssistantAimHeaderAction)
enum class AssistantAimHeaderAction {
  kCloseButtonTapped = 0,
  kNewThreadTapped = 1,
  kThreadHistoryTapped = 2,
  kMyActivityTapped = 3,
  kHelpTapped = 4,
  kMaxValue = kHelpTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSicBottomSheetHeaderAction)

// Events and actions on the Assistant AIM first run experience (FRE) notice
// alert.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AssistantAimFRENoticeAction)
enum class AssistantAimFRENoticeAction {
  kShown = 0,
  kAccepted = 1,
  kLearnMoreTapped = 2,
  kMaxValue = kLearnMoreTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSicBottomSheetFRENoticeAction)

#endif  // IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_CONSTANTS_H_
