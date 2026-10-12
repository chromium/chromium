// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/ntp_tiles/constants.h"
#import "components/ntp_tiles/features.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/pinned_site_action.h"

namespace {

/// User action name for tapping the "Add site" button.
const char kAddSiteUserActionName[] = "Suggestions.Button.AddItem";

/// User action name for drag-and-drop operations on the pinned sites.
const char kReorderUserActionName[] = "Suggestions.Drag.ReorderItem";

/// Prefix for user action name for drag-and-drop operations on the pinned
/// sites.
const char kSnackbarUndoUserActionName[] = "Suggestions.SnackBar.Undo";
/// Suffixes for user action name for drag-and-drop operations on the pinned
/// sites.
const char kUndoPinSuffix[] = "PinItem";
const char kUndoUnpinSuffix[] = "UnpinItem";

/// User action name for unpinning the AIM tile.
constexpr char kAimTileUnpinnedUserActionName[] = "Suggestions.AIM.Unpinned";

/// User action name for undoing the unpinning of the AIM tile.
constexpr char kAimTileUndoUnpinUserActionName[] = "Suggestions.AIM.UndoUnpin";

/// Histogram prefix for actions on the pinned site form.
const char kPinnedSiteFormHistogramPrefix[] = "IOS.MostVisited.PinnedSiteForm.";

/// Histogram name for the destination index when the AIM tile is moved.
constexpr char kAimTileMovedToIndexHistogram[] =
    "IOS.MostVisited.AIM.MovedToIndex";

/// Histogram name for the index of the AIM tile when tapped.
constexpr char kAimTileTappedAtIndexHistogram[] =
    "IOS.MostVisited.AIM.TapIndex";

/// Histogram name for the impression index of the AIM tile.
constexpr char kAimTileImpressionAtIndexHistogram[] =
    "IOS.MostVisited.AIM.ImpressionIndex";

}  // namespace

using base::UserMetricsAction;

void RecordAddSiteUserAction() {
  base::RecordAction(UserMetricsAction(kAddSiteUserActionName));
}

void RecordReorderUserAction() {
  base::RecordAction(UserMetricsAction(kReorderUserActionName));
}

void RecordSnackbarUndoUserAction(bool undo_pin) {
  std::string action_name = std::string(kSnackbarUndoUserActionName) +
                            (undo_pin ? kUndoPinSuffix : kUndoUnpinSuffix);
  base::RecordAction(UserMetricsAction(action_name.c_str()));
}

void RecordPinnedSiteFormUserAction(PinnedSiteAction form,
                                    MostVisitedPinSiteFormUserAction action) {
  std::string suffix;
  switch (form) {
    case PinnedSiteAction::kCreate:
      suffix = "AddSite";
      break;
    case PinnedSiteAction::kModify:
      suffix = "EditSite";
      break;
  }
  base::UmaHistogramEnumeration(kPinnedSiteFormHistogramPrefix + suffix,
                                action);
}

void RecordAimTileUnpinnedUserAction() {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    base::RecordAction(UserMetricsAction(kAimTileUnpinnedUserActionName));
  }
}

void RecordAimTileUndoUnpinUserAction() {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    base::RecordAction(UserMetricsAction(kAimTileUndoUnpinUserActionName));
  }
}

void RecordAimTileMovedToIndex(int index) {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    base::UmaHistogramExactLinear(kAimTileMovedToIndexHistogram, index,
                                  ntp_tiles::kMaxNumTiles);
  }
}

void RecordAimTileTappedAtIndex(int index) {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    base::UmaHistogramExactLinear(kAimTileTappedAtIndexHistogram, index,
                                  ntp_tiles::kMaxNumTiles);
  }
}

void RecordAimTileImpressionAtIndex(int index) {
  if (ntp_tiles::GetAimButtonRefactorArm() ==
      ntp_tiles::AimButtonRefactorArm::kAimAsMvt) {
    base::UmaHistogramExactLinear(kAimTileImpressionAtIndexHistogram, index,
                                  ntp_tiles::kMaxNumTiles);
  }
}
