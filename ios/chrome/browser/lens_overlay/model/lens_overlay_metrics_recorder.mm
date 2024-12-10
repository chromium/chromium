// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_metrics_recorder.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/timer/elapsed_timer.h"
#import "components/lens/lens_overlay_metrics.h"
#import "components/lens/lens_overlay_mime_type.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/shared/model/utils/mime_type_util.h"
#import "ios/web/public/web_state.h"

namespace {

/// Returns the `lens::MimeType` of the `web_state`.
lens::MimeType MimeTypeFromWebState(web::WebState* web_state) {
  if (!web_state) {
    return lens::MimeType::kUnknown;
  }

  const std::string& mime_type = web_state->GetContentsMimeType();
  if (mime_type == kHyperTextMarkupLanguageMimeType) {
    return lens::MimeType::kHtml;
  } else if (mime_type == kAdobePortableDocumentFormatMimeType) {
    return lens::MimeType::kPdf;
  } else if (mime_type == kTextMimeType) {
    return lens::MimeType::kPlainText;
  } else {
    return lens::MimeType::kUnknown;
  }
}

}  // namespace

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
  /// The source ID of the webState where lens was invoked on.
  int64_t _sourceID;
  /// The mime type of the webState where lens was invoked on.
  lens::MimeType _mimeType;
}

- (instancetype)initWithEntrypoint:(LensOverlayEntrypoint)entrypoint
                associatedWebState:(web::WebState*)associatedWebState {
  self = [super init];
  if (self) {
    _invocationSource = lens::InvocationSourceFromEntrypoint(entrypoint);
    _sourceID = associatedWebState
                    ? ukm::GetSourceIdForWebStateDocument(associatedWebState)
                    : ukm::kInvalidSourceId;
    _mimeType = MimeTypeFromWebState(associatedWebState);
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

  if (!lensOverlayInForeground && _foregroundTime != base::TimeTicks()) {
    // Add the foreground duration and reset the timer.
    _foregroundDuration =
        _foregroundDuration + (base::TimeTicks::Now() - _foregroundTime);
    _foregroundTime = base::TimeTicks();
  } else {
    _foregroundTime = base::TimeTicks::Now();
  }
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
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Consent.Accepted"));
  lens::RecordPermissionUserAction(
      lens::LensPermissionUserAction::kAcceptButtonPressed, _invocationSource);
  [self recordFirstInteraction:lens::LensOverlayFirstInteractionType::
                                   kPermissionDialog];
}
- (void)recordPermissionsDenied {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Consent.Denied"));
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
  lens::RecordInvocation(_invocationSource, _mimeType);
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
  lens::RecordUKMSessionEndMetrics(
      _sourceID, _invocationSource, _searchPerformedInSession, sessionDuration,
      _mimeType, _foregroundDuration, generatedTabCount);
}

@end
