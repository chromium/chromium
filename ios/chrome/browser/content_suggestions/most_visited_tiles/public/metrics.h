// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_METRICS_H_

enum class PinnedSiteAction;

// LINT.IfChange(MostVisitedPinSiteFormUserAction)
/// Enum for the "IOS.MostVisited.PinnedSiteForm.*" histograms; logs user's
/// action that dismissed the pinned site form.
enum class MostVisitedPinSiteFormUserAction {
  /// User dismisses the form without attempting to apply any changes.
  kDismissImmediately = 0,
  /// User applies changes and succeeds on the first try.
  kApplyImmediately = 1,
  /// User dismisses the form after failing to apply changes.
  kDismissAfterFailure = 2,
  /// User applies changes and succeeds on a retry after failure.
  kApplyAfterFailure = 3,
  kMaxValue = kApplyAfterFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSMostVisitedPinSiteFormUserAction)

/// Records user actions on the "Add Site" button and form.
void RecordAddSiteUserAction();

/// Records user actions on reordering pinned sites on the most visited tiles.
void RecordReorderUserAction();

/// Records user reverting pin actions on the snackbar. To undo a pinnin action,
/// `undo_pin` should be true.
void RecordSnackbarUndoUserAction(bool undo_pin);

/// Records user actions on the form to pin site.
void RecordPinnedSiteFormUserAction(PinnedSiteAction form,
                                    MostVisitedPinSiteFormUserAction action);

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_PUBLIC_METRICS_H_
