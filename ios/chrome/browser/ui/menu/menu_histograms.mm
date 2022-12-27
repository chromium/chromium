// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/menu_histograms.h"

#import "base/metrics/histogram_functions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Histogram for tracking menu scenario started.
const char kMenuEntryPointsHistogram[] = "Mobile.ContextMenu.EntryPoints";

// Histograms for tracking actions performed on given menus.
const char kBookmarkEntryActionsHistogram[] =
    "Mobile.ContextMenu.BookmarkEntry.Actions";
const char kBookmarkFolderActionsHistogram[] =
    "Mobile.ContextMenu.BookmarkFolder.Actions";
const char kReadingListEntryActionsHistogram[] =
    "Mobile.ContextMenu.ReadingListEntry.Actions";
const char kRecentTabsHeaderActionsHistogram[] =
    "Mobile.ContextMenu.RecentTabsHeader.Actions";
const char kRecentTabsEntryActionsHistogram[] =
    "Mobile.ContextMenu.RecentTabsEntry.Actions";
const char kHistoryEntryActionsHistogram[] =
    "Mobile.ContextMenu.HistoryEntry.Actions";
const char kMostVisitedEntryActionsHistogram[] =
    "Mobile.ContextMenu.MostVisitedEntry.Actions";
const char kTabGridActionsHistogram[] = "Mobile.ContextMenu.TabGrid.Actions";
const char kTabGridAddToActionsHistogram[] =
    "Mobile.ContextMenu.TabGridAddTo.Actions";
const char kTabGridEditActionsHistogram[] =
    "Mobile.ContextMenu.TabGridEdit.Actions";
const char kTabGridSearchResultHistogram[] =
    "Mobile.ContextMenu.TabGridSearchResult.Actions";
const char KContextMenuImageActionsHistogram[] =
    "Mobile.ContextMenu.WebImage.Actions";
const char KContextMenuImageLinkActionsHistogram[] =
    "Mobile.ContextMenu.WebImageLink.Actions";
const char KContextMenuLinkActionsHistogram[] =
    "Mobile.ContextMenu.WebLink.Actions";
const char kToolbarMenuActionsHistogram[] =
    "Mobile.ContextMenu.Toolbar.Actions";
const char kOmniboxMostVisitedEntryActionsHistogram[] =
    "Mobile.ContextMenu.OmniboxMostVisitedEntry.Actions";
const char kPinnedTabsEntryActionsHistogram[] =
    "Mobile.ContextMenu.PinnedTabsEntry.Actions";
}  // namespace

void RecordMenuShown(MenuScenarioHistogram scenario) {
  base::UmaHistogramEnumeration(kMenuEntryPointsHistogram, scenario);
}

const char* GetActionsHistogramName(MenuScenarioHistogram scenario) {
  switch (scenario) {
    case MenuScenarioHistogram::kHistoryEntry:
      return kHistoryEntryActionsHistogram;
    case MenuScenarioHistogram::kBookmarkEntry:
      return kBookmarkEntryActionsHistogram;
    case MenuScenarioHistogram::kReadingListEntry:
      return kReadingListEntryActionsHistogram;
    case MenuScenarioHistogram::kRecentTabsEntry:
      return kRecentTabsEntryActionsHistogram;
    case MenuScenarioHistogram::kRecentTabsHeader:
      return kRecentTabsHeaderActionsHistogram;
    case MenuScenarioHistogram::kMostVisitedEntry:
      return kMostVisitedEntryActionsHistogram;
    case MenuScenarioHistogram::kBookmarkFolder:
      return kBookmarkFolderActionsHistogram;
    case MenuScenarioHistogram::kContextMenuImage:
      return KContextMenuImageActionsHistogram;
    case MenuScenarioHistogram::kContextMenuImageLink:
      return KContextMenuImageLinkActionsHistogram;
    case MenuScenarioHistogram::kContextMenuLink:
      return KContextMenuLinkActionsHistogram;
    case MenuScenarioHistogram::kTabGridEntry:
    case MenuScenarioHistogram::kThumbStrip:
      return kTabGridActionsHistogram;
    case MenuScenarioHistogram::kTabGridAddTo:
      return kTabGridAddToActionsHistogram;
    case MenuScenarioHistogram::kTabGridEdit:
      return kTabGridEditActionsHistogram;
    case MenuScenarioHistogram::kTabGridSearchResult:
      return kTabGridSearchResultHistogram;
    case MenuScenarioHistogram::kToolbarMenu:
      return kToolbarMenuActionsHistogram;
    case MenuScenarioHistogram::kOmniboxMostVisitedEntry:
      return kOmniboxMostVisitedEntryActionsHistogram;
    case MenuScenarioHistogram::kPinnedTabsEntry:
      return kPinnedTabsEntryActionsHistogram;
  }
}
