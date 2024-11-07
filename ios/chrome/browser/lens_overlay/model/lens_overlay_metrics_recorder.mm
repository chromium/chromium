// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_metrics_recorder.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/timer/elapsed_timer.h"
#import "components/lens/lens_overlay_metrics.h"
#import "components/lens/lens_overlay_page_content_mime_type.h"

@implementation LensOverlayMetricsRecorder {
  /// Whether a lens request has been performed during this session.
  BOOL _searchPerformedInSession;
  /// Whether lens overlay is in foreground.
  BOOL _lensOverlayInForeground;
  /// The time at which the overlay was invoked.
  base::ElapsedTimer _invocationTime;
  /// The time at which the overlay UI was `shown`. null when hidden.
  base::TimeTicks _foregroundTime;
  /// The total foregroud duration since invoked.
  base::TimeDelta _foregroundDuration;
  /// Whether the first interaction has been recorded.
  BOOL _firstInteractionRecorded;
  /// The invocation source.
  lens::LensOverlayInvocationSource _invocationSource;
  /// The associated source ID.
  int64_t _sourceID;
}

- (instancetype)initWithEntrypoint:(LensOverlayEntrypoint)entrypoint
                          sourceID:(int64_t)sourceID {
  self = [super init];
  if (self) {
    _invocationSource = lens::InvocationSourceFromEntrypoint(entrypoint);
    _sourceID = sourceID;
    _firstInteractionRecorded = NO;
    _searchPerformedInSession = NO;
    _invocationTime = base::ElapsedTimer();
    _foregroundDuration = base::TimeDelta();
  }

  return self;
}

- (void)setLensOverlayInForeground:(BOOL)lensOverlayInForeground {
  // Only trigger a side effect when there is a state change.
  if (_lensOverlayInForeground == lensOverlayInForeground) {
    return;
  }

  if (!lensOverlayInForeground) {
    // Add the foreground duration and reset the timer.
    _foregroundDuration =
        _foregroundDuration + (base::TimeTicks::Now() - _foregroundTime);
  }
  _foregroundTime = base::TimeTicks();
}

- (void)recordOverflowMenuOpened {
  [self
      recordFirstInteraction:lens::LensOverlayFirstInteractionType::kLensMenu];
}

/// Records the first interaction time.
- (void)recordFirstInteraction:
    (lens::LensOverlayFirstInteractionType)firstInteractionType {
  if (_firstInteractionRecorded) {
    return;
  }
  _firstInteractionRecorded = YES;

  lens::RecordTimeToFirstInteraction(_invocationSource,
                                     _invocationTime.Elapsed(),
                                     firstInteractionType, _sourceID);
}

- (void)recordLensOverlayConsentShown {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Consent.Shown"));
}

- (void)recordLensOverlayClosed {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Closed"));
}

- (void)recordPermissionsLinkOpen {
  lens::RecordPermissionUserAction(lens::LensPermissionUserAction::kLinkOpened,
                                   _invocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
}

- (void)recordResultLoadedWithTextSelection:(BOOL)isTextSelection {
  _searchPerformedInSession = YES;
  [self recordFirstInteraction:
            isTextSelection
                ? lens::LensOverlayFirstInteractionType::kTextSelect
                : lens::LensOverlayFirstInteractionType::kRegionSelect];
}

- (void)recordPermissionsAccepted {
  lens::RecordPermissionUserAction(
      lens::LensPermissionUserAction::kAcceptButtonPressed, _invocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
}
- (void)recordPermissionsDenied {
  lens::RecordPermissionUserAction(
      lens::LensPermissionUserAction::kCancelButtonPressed, _invocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
}

- (void)recordPermissionRequestedToBeShown {
  lens::RecordPermissionRequestedToBeShown(true, _invocationSource);
}

/// Metrics recorded on lens overlay dismissal.
- (void)recordDismissalMetricsWithSource:
            (lens::LensOverlayDismissalSource)dismissalSource
                       generatedTabCount:(NSInteger)generatedTabCount {
  [self recordLensOverlayClosed];

  // Invocation metrics.
  lens::RecordInvocation(_invocationSource, lens::PageContentMimeType::kNone);
  lens::RecordInvocationResultedInSearch(_invocationSource,
                                         _searchPerformedInSession);
  // Dismissal metric.
  lens::RecordDismissal(dismissalSource);

  // Session foreground duration metrics.
  if (_foregroundTime != base::TimeTicks()) {
    _foregroundDuration =
        _foregroundDuration + (base::TimeTicks::Now() - _foregroundTime);
  }
  lens::RecordSessionForegroundDuration(_invocationSource, _foregroundDuration);

  // Session duration metrics.
  base::TimeDelta sessionDuration = _invocationTime.Elapsed();
  lens::RecordSessionDuration(_invocationSource, sessionDuration);

  // Records number of tabs opened by the lens overlay during session.
  lens::RecordGeneratedTabCount((int)generatedTabCount);

  // Session end UKM metrics.
  lens::RecordUKMSessionEndMetrics(_sourceID, _invocationSource,
                                   _searchPerformedInSession, sessionDuration,
                                   _foregroundDuration, generatedTabCount);
}

@end
