// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_recorder.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class AssistantAIMMetricsRecorderTest : public PlatformTest {
 protected:
  AssistantAIMMetricsRecorderTest()
      : recorder_([[AssistantAIMMetricsRecorder alloc] init]) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AssistantAIMMetricsRecorder* recorder_;
  base::HistogramTester histogram_tester_;
};

// Tests that starting and immediately dismissing a session without interaction
// logs entrypoint, unengaged duration, and immediate dismissal.
TEST_F(AssistantAIMMetricsRecorderTest, ImmediateUnengagedDismissal) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  histogram_tester_.ExpectUniqueSample(
      kAssistantAimEntrypointHistogram,
      AssistantAimEntrypoint::kLocationBarAction, 1);

  task_environment_.FastForwardBy(base::Seconds(1));
  [recorder_ recordClosedSnackbarResolvedWithUndo:NO];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueSample(
      kAssistantAimStateTransitionHistogram,
      AssistantAimStateTransition::kDismissedCloseButton, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadEngagementHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(kAssistantAimImmediateDismissalHistogram,
                                       true, 1);
  histogram_tester_.ExpectUniqueTimeSample(kAssistantAimTimeSpentHistogram,
                                           base::Seconds(1), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      kAssistantAimTimeSpentAbandonedHistogram, base::Seconds(1), 1);
  histogram_tester_.ExpectTotalCount(kAssistantAimTimeSpentEngagedHistogram, 0);
}

// Tests that an unengaged session lasting longer than the immediate dismissal
// threshold is not marked as an immediate dismissal and pauses hidden time.
TEST_F(AssistantAIMMetricsRecorderTest, VisibilityPausesActiveTimeSpent) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kSessionRestore];

  task_environment_.FastForwardBy(base::Seconds(2));
  [recorder_ recordVisibilityChanged:NO];
  // Time while hidden should not count toward `TimeSpent`.
  task_environment_.FastForwardBy(base::Seconds(10));
  [recorder_ recordVisibilityChanged:YES];
  task_environment_.FastForwardBy(base::Seconds(3));

  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueTimeSample(kAssistantAimTimeSpentHistogram,
                                           base::Seconds(5), 1);
  histogram_tester_.ExpectUniqueSample(kAssistantAimImmediateDismissalHistogram,
                                       false, 1);
}

// Tests that expanding the sheet to large detent records positive engagement
// and logs engaged duration at session end.
TEST_F(AssistantAIMMetricsRecorderTest,
       DetentExpansionRecordsPositiveEngagement) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kAskAboutCurrentPage];

  [recorder_ recordDetentChanged:AssistantContainerDetent::kLarge];
  task_environment_.FastForwardBy(base::Seconds(4));
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(kAssistantAimStateTransitionHistogram,
                                      AssistantAimStateTransition::kLargeSheet,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimStateTransitionHistogram,
      AssistantAimStateTransition::kDismissedOther, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadEngagementHistogram, true, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      kAssistantAimTimeSpentEngagedHistogram, base::Seconds(4), 1);
  histogram_tester_.ExpectTotalCount(kAssistantAimTimeSpentAbandonedHistogram,
                                     0);
}

// Tests that tapping Undo on the close snackbar records the undo metric and
// transition state.
TEST_F(AssistantAIMMetricsRecorderTest, CloseUndoRestoresState) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordClosedSnackbarResolvedWithUndo:YES];

  histogram_tester_.ExpectUniqueSample(kAssistantAimCloseUndoTappedHistogram,
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimStateTransitionHistogram,
      AssistantAimStateTransition::kRestoredWithUndo, 1);
}

// Tests that tapping the close button logs `kCloseButtonTapped` without
// marking positive engagement, whereas non-close header and menu actions mark
// positive engagement.
TEST_F(AssistantAIMMetricsRecorderTest,
       HeaderActionsRecordMetricsAndEngagement) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordHeaderAction:AssistantAimHeaderAction::kCloseButtonTapped];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueSample(
      kAssistantAimHeaderActionHistogram,
      AssistantAimHeaderAction::kCloseButtonTapped, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadEngagementHistogram, false, 1);

  // Start a second session and exercise non-close header actions.
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];
  [recorder_ recordHeaderAction:AssistantAimHeaderAction::kThreadHistoryTapped];
  [recorder_ recordHeaderAction:AssistantAimHeaderAction::kMyActivityTapped];
  [recorder_ recordHeaderAction:AssistantAimHeaderAction::kHelpTapped];
  [recorder_ recordHeaderAction:AssistantAimHeaderAction::kNewThreadTapped];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(
      kAssistantAimHeaderActionHistogram,
      AssistantAimHeaderAction::kThreadHistoryTapped, 1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimHeaderActionHistogram,
      AssistantAimHeaderAction::kMyActivityTapped, 1);
  histogram_tester_.ExpectBucketCount(kAssistantAimHeaderActionHistogram,
                                      AssistantAimHeaderAction::kHelpTapped, 1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimHeaderActionHistogram,
      AssistantAimHeaderAction::kNewThreadTapped, 1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimSessionHadEngagementHistogram, true, 1);
}

// Tests that clicking a link and expanding back from minimized state records
// `ReturnedAfterLinkClick` as true, counts link clicks per session, and resets
// link click state when a new session starts.
TEST_F(AssistantAIMMetricsRecorderTest, LinkClickedAndReturnFromMinimized) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordLinkClicked];
  [recorder_ recordDetentChanged:AssistantContainerDetent::kMinimized];
  [recorder_ recordDetentChanged:AssistantContainerDetent::kMedium];
  [recorder_ recordLinkClicked];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueSample(kAssistantAimLinkClickedHistogram, true,
                                       2);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimLinksClickedPerSessionHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadLinkClickHistogram, true, 1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimReturnedAfterLinkClickHistogram, true, 1);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimReturnedAfterLinkClickHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadEngagementHistogram, true, 1);

  // Starting a new session should reset `_linksClickedCount` and
  // `_awaitingReturnFromLinkClick`.
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];
  [recorder_ recordDetentChanged:AssistantContainerDetent::kLarge];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(
      kAssistantAimLinksClickedPerSessionHistogram, 0, 1);
  histogram_tester_.ExpectBucketCount(kAssistantAimSessionHadLinkClickHistogram,
                                      false, 1);
  histogram_tester_.ExpectTotalCount(
      kAssistantAimReturnedAfterLinkClickHistogram, 2);
}

// Tests that dismissing the sheet while awaiting return from a link click logs
// `ReturnedAfterLinkClick` as false.
TEST_F(AssistantAIMMetricsRecorderTest, LinkClickedWithoutReturn) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordLinkClicked];
  [recorder_ recordDetentChanged:AssistantContainerDetent::kMinimized];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueSample(
      kAssistantAimReturnedAfterLinkClickHistogram, false, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimLinksClickedPerSessionHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadLinkClickHistogram, true, 1);
}

// Tests that submitting follow-up queries increments the per-session follow-up
// and attachment counts, records whether each follow-up had an attachment, and
// marks positive engagement at session end.
TEST_F(AssistantAIMMetricsRecorderTest, FollowupSubmissionsPerSession) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kAskAboutCurrentPage];

  [recorder_ recordFollowupSubmittedWithAttachmentCount:2];
  [recorder_ recordFollowupSubmittedWithAttachmentCount:0];
  [recorder_ recordFollowupSubmittedWithAttachmentCount:1];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(
      kAssistantAimFollowupHadAttachmentHistogram, true, 2);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimFollowupHadAttachmentHistogram, false, 1);
  histogram_tester_.ExpectTotalCount(
      kAssistantAimFollowupHadAttachmentHistogram, 3);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimFollowupsPerSessionHistogram, 3, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimAttachmentsPerSessionHistogram, 3, 1);
  histogram_tester_.ExpectUniqueSample(
      kAssistantAimSessionHadEngagementHistogram, true, 1);

  // Starting a new session should reset `_followupsSubmittedCount` and
  // `_attachmentsSubmittedCount`.
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kAskAboutCurrentPage];
  [recorder_ recordFollowupSubmittedWithAttachmentCount:1];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(kAssistantAimFollowupsPerSessionHistogram,
                                      1, 1);
  histogram_tester_.ExpectTotalCount(kAssistantAimFollowupsPerSessionHistogram,
                                     2);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimAttachmentsPerSessionHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(
      kAssistantAimAttachmentsPerSessionHistogram, 2);
}

// Tests that removing an auto-added tab logs `AutoAddedTabRemoved` as true when
// the session ends, and logs false when a session ends without removing one.
TEST_F(AssistantAIMMetricsRecorderTest, AutoAddedTabRemoved) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordAutoAddedTabRemoved];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectUniqueSample(
      kAssistantAimAutoAddedTabRemovedHistogram, true, 1);

  // Starting a new session should reset `_autoAddedTabRemoved` and log false if
  // no auto-added tab is removed during the session.
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(kAssistantAimAutoAddedTabRemovedHistogram,
                                      true, 1);
  histogram_tester_.ExpectBucketCount(kAssistantAimAutoAddedTabRemovedHistogram,
                                      false, 1);
  histogram_tester_.ExpectTotalCount(kAssistantAimAutoAddedTabRemovedHistogram,
                                     2);
}

// Tests that presentation and actions on the FRE notice alert log
// `FRENoticeAction`.
TEST_F(AssistantAIMMetricsRecorderTest, FRENoticeActions) {
  [recorder_ recordSessionStartedWithEntrypoint:AssistantAimEntrypoint::
                                                    kLocationBarAction];

  [recorder_ recordFRENoticeAction:AssistantAimFRENoticeAction::kShown];
  [recorder_
      recordFRENoticeAction:AssistantAimFRENoticeAction::kLearnMoreTapped];
  [recorder_ recordFRENoticeAction:AssistantAimFRENoticeAction::kShown];
  [recorder_ recordFRENoticeAction:AssistantAimFRENoticeAction::kAccepted];
  [recorder_ recordSessionEnded];

  histogram_tester_.ExpectBucketCount(kAssistantAimFRENoticeActionHistogram,
                                      AssistantAimFRENoticeAction::kShown, 2);
  histogram_tester_.ExpectBucketCount(
      kAssistantAimFRENoticeActionHistogram,
      AssistantAimFRENoticeAction::kLearnMoreTapped, 1);
  histogram_tester_.ExpectBucketCount(kAssistantAimFRENoticeActionHistogram,
                                      AssistantAimFRENoticeAction::kAccepted,
                                      1);
  histogram_tester_.ExpectTotalCount(kAssistantAimFRENoticeActionHistogram, 4);
}

}  // namespace
