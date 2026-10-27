// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_recorder.h"

#import <optional>

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/timer/elapsed_timer.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/shared/metrics/assistant_aim_metrics_constants.h"

namespace {

// Maximum active visible duration for an unengaged session to be considered an
// immediate dismissal.
constexpr base::TimeDelta kImmediateDismissalThreshold = base::Seconds(3);

// Maps `AssistantContainerDetent` to `AssistantAimStateTransition`.
AssistantAimStateTransition AssistantAimStateTransitionFromDetent(
    AssistantContainerDetent detent) {
  switch (detent) {
    case AssistantContainerDetent::kMinimized:
      return AssistantAimStateTransition::kMinimizedSheet;
    case AssistantContainerDetent::kMedium:
      return AssistantAimStateTransition::kMediumSheet;
    case AssistantContainerDetent::kLarge:
      return AssistantAimStateTransition::kLargeSheet;
  }
}

}  // namespace

@implementation AssistantAIMMetricsRecorder {
  // Whether a metrics session is currently active.
  BOOL _sessionActive;
  // Whether the bottom sheet is currently visible to the user.
  BOOL _isVisible;
  // Timer tracking the current active visible interval.
  base::ElapsedTimer _visibleTimer;
  // Total accumulated visible duration across visibility changes.
  base::TimeDelta _accumulatedVisibleDuration;
  // Most recently recorded sheet detent.
  std::optional<AssistantContainerDetent> _currentDetent;
  // Whether the user performed at least one qualifying positive engagement.
  BOOL _hasPositiveEngagement;
  // Whether the session was explicitly closed via the close button.
  BOOL _closedByButton;
  // Number of outbound links clicked during the active session.
  int _linksClickedCount;
  // Whether the sheet was minimized due to a link click and is awaiting return.
  BOOL _awaitingReturnFromLinkClick;
  // Number of follow-up queries submitted during the active session.
  int _followupsSubmittedCount;
  // Total number of attachments submitted with follow-up queries during the
  // active session.
  int _attachmentsSubmittedCount;
  // Whether the user explicitly removed an auto-added tab during the active
  // session.
  BOOL _autoAddedTabRemoved;
}

#pragma mark - Public

- (void)recordSessionStartedWithEntrypoint:(AssistantAimEntrypoint)entrypoint {
  [self resetSessionState];
  _sessionActive = YES;
  _isVisible = YES;
  _visibleTimer = base::ElapsedTimer();

  base::UmaHistogramEnumeration(kAssistantAimEntrypointHistogram, entrypoint);
}

- (void)recordVisibilityChanged:(BOOL)visible {
  if (!_sessionActive || _isVisible == visible) {
    return;
  }
  if (visible) {
    _isVisible = YES;
    _visibleTimer = base::ElapsedTimer();
  } else {
    _accumulatedVisibleDuration += _visibleTimer.Elapsed();
    _isVisible = NO;
  }
}

- (void)recordDetentChanged:(AssistantContainerDetent)detent {
  if (!_sessionActive || _currentDetent == detent) {
    return;
  }

  if (detent != AssistantContainerDetent::kMinimized) {
    _hasPositiveEngagement = YES;
    if (_awaitingReturnFromLinkClick) {
      base::UmaHistogramBoolean(kAssistantAimReturnedAfterLinkClickHistogram,
                                true);
      _awaitingReturnFromLinkClick = NO;
    }
  }

  _currentDetent = detent;
  base::UmaHistogramEnumeration(kAssistantAimStateTransitionHistogram,
                                AssistantAimStateTransitionFromDetent(detent));
}

- (void)recordClosedSnackbarResolvedWithUndo:(BOOL)didUndo {
  base::UmaHistogramBoolean(kAssistantAimCloseUndoTappedHistogram, didUndo);
  _closedByButton = !didUndo;
  if (didUndo && _sessionActive) {
    base::UmaHistogramEnumeration(
        kAssistantAimStateTransitionHistogram,
        AssistantAimStateTransition::kRestoredWithUndo);
  }
}

- (void)recordHeaderAction:(AssistantAimHeaderAction)action {
  if (!_sessionActive) {
    return;
  }
  if (action != AssistantAimHeaderAction::kCloseButtonTapped) {
    _hasPositiveEngagement = YES;
  }
  base::UmaHistogramEnumeration(kAssistantAimHeaderActionHistogram, action);
}

- (void)recordLinkClicked {
  if (!_sessionActive) {
    return;
  }
  _linksClickedCount++;
  _hasPositiveEngagement = YES;
  _awaitingReturnFromLinkClick = YES;
  base::UmaHistogramBoolean(kAssistantAimLinkClickedHistogram, true);
}

- (void)recordFollowupSubmittedWithAttachmentCount:(NSUInteger)attachmentCount {
  if (!_sessionActive) {
    return;
  }
  _followupsSubmittedCount++;
  _attachmentsSubmittedCount += attachmentCount;
  _hasPositiveEngagement = YES;
  base::UmaHistogramBoolean(kAssistantAimFollowupHadAttachmentHistogram,
                            attachmentCount > 0);
}

- (void)recordAutoAddedTabRemoved {
  if (!_sessionActive) {
    return;
  }
  _autoAddedTabRemoved = YES;
}

- (void)recordFRENoticeAction:(AssistantAimFRENoticeAction)action {
  if (!_sessionActive) {
    return;
  }
  base::UmaHistogramEnumeration(kAssistantAimFRENoticeActionHistogram, action);
}

- (void)recordSessionEnded {
  if (!_sessionActive) {
    return;
  }
  [self recordVisibilityChanged:NO];

  AssistantAimStateTransition dismissalState =
      _closedByButton ? AssistantAimStateTransition::kDismissedCloseButton
                      : AssistantAimStateTransition::kDismissedOther;
  base::UmaHistogramEnumeration(kAssistantAimStateTransitionHistogram,
                                dismissalState);
  if (_awaitingReturnFromLinkClick) {
    base::UmaHistogramBoolean(kAssistantAimReturnedAfterLinkClickHistogram,
                              false);
    _awaitingReturnFromLinkClick = NO;
  }

  base::UmaHistogramCounts100(kAssistantAimFollowupsPerSessionHistogram,
                              _followupsSubmittedCount);
  base::UmaHistogramCounts100(kAssistantAimAttachmentsPerSessionHistogram,
                              _attachmentsSubmittedCount);
  base::UmaHistogramCounts100(kAssistantAimLinksClickedPerSessionHistogram,
                              _linksClickedCount);
  base::UmaHistogramBoolean(kAssistantAimSessionHadLinkClickHistogram,
                            _linksClickedCount > 0);
  base::UmaHistogramBoolean(kAssistantAimAutoAddedTabRemovedHistogram,
                            _autoAddedTabRemoved);
  [self recordSessionEngagementAndDurationMetrics];

  _sessionActive = NO;
}

#pragma mark - Private

// Resets all session-scoped state flags and timers.
- (void)resetSessionState {
  _sessionActive = NO;
  _isVisible = NO;
  _accumulatedVisibleDuration = base::TimeDelta();
  _currentDetent = std::nullopt;
  _hasPositiveEngagement = NO;
  _closedByButton = NO;
  _linksClickedCount = 0;
  _awaitingReturnFromLinkClick = NO;
  _followupsSubmittedCount = 0;
  _attachmentsSubmittedCount = 0;
  _autoAddedTabRemoved = NO;
}

// Logs engagement boolean, active visible time, and immediate dismissal.
- (void)recordSessionEngagementAndDurationMetrics {
  base::UmaHistogramBoolean(kAssistantAimSessionHadEngagementHistogram,
                            _hasPositiveEngagement);

  base::UmaHistogramLongTimes(kAssistantAimTimeSpentHistogram,
                              _accumulatedVisibleDuration);
  if (_hasPositiveEngagement) {
    base::UmaHistogramLongTimes(kAssistantAimTimeSpentEngagedHistogram,
                                _accumulatedVisibleDuration);
  } else {
    base::UmaHistogramLongTimes(kAssistantAimTimeSpentAbandonedHistogram,
                                _accumulatedVisibleDuration);
  }

  BOOL isImmediateDismissal =
      !_hasPositiveEngagement &&
      (_accumulatedVisibleDuration < kImmediateDismissalThreshold);
  base::UmaHistogramBoolean(kAssistantAimImmediateDismissalHistogram,
                            isImmediateDismissal);
}

@end
