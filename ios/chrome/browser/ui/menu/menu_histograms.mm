// Copyright 2020 The Chromium Authors. All rights reserved.
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
}  // namespace

void RecordMenuShown(MenuScenario scenario) {
  base::UmaHistogramEnumeration(kMenuEntryPointsHistogram, scenario);
}

const char* GetActionsHistogramName(MenuScenario scenario) {
  switch (scenario) {
    case MenuScenario::kHistoryEntry:
      return kHistoryEntryActionsHistogram;
    case MenuScenario::kBookmarkEntry:
      return kBookmarkEntryActionsHistogram;
    case MenuScenario::kReadingListEntry:
      return kReadingListEntryActionsHistogram;
    case MenuScenario::kRecentTabsEntry:
      return kRecentTabsEntryActionsHistogram;
    case MenuScenario::kRecentTabsHeader:
      return kRecentTabsHeaderActionsHistogram;
    case MenuScenario::kMostVisitedEntry:
      return kMostVisitedEntryActionsHistogram;
    case MenuScenario::kBookmarkFolder:
      return kBookmarkFolderActionsHistogram;
    case MenuScenario::kContextMenuImage:
      return KContextMenuImageActionsHistogram;
    case MenuScenario::kContextMenuImageLink:
      return KContextMenuImageLinkActionsHistogram;
    case MenuScenario::kContextMenuLink:
      return KContextMenuLinkActionsHistogram;
    case MenuScenario::kTabGridEntry:
    case MenuScenario::kThumbStrip:
      return kTabGridActionsHistogram;
    case MenuScenario::kTabGridAddTo:
      return kTabGridAddToActionsHistogram;
    case MenuScenario::kTabGridEdit:
      return kTabGridEditActionsHistogram;
    case MenuScenario::kTabGridSearchResult:
      return kTabGridSearchResultHistogram;
    case MenuScenario::kToolbarMenu:
      return kToolbarMenuActionsHistogram;
  }
}
