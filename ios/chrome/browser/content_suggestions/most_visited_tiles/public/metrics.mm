// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/public/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
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

/// Histogram prefix for actions on the pinned site form.
const char kPinnedSiteFormHistogramPrefix[] = "IOS.MostVisited.PinnedSiteForm.";

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
