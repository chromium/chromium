// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/menu_histograms.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"

namespace {
// Histogram for tracking menu scenario started.
const char kMenuEntryPointsHistogram[] = "Mobile.ContextMenu.EntryPoints";

// Histograms for tracking actions performed on given menus.
// LINT.IfChange
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
const char kTabStripEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabStrip.Actions";
const char kInactiveTabsEntryActionsHistogram[] =
    "Mobile.ContextMenu.InactiveTabsEntry.Actions";
const char kTabGroupEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabGroupEntry.Actions";
const char kTabGroupViewEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabGroupViewEntry.Actions";
const char kAutofillManualFallbackAllPasswordsEntryActionsHistogram[] =
    "Mobile.ContextMenu.AutofillManualFallbackAllPasswordsEntry.Actions";
const char kAutofillManualFallbackPasswordEntryActionsHistogram[] =
    "Mobile.ContextMenu.AutofillManualFallbackPasswordEntry.Actions";
const char kAutofillManualFallbackPaymentEntryActionsHistogram[] =
    "Mobile.ContextMenu.AutofillManualFallbackPaymentEntry.Actions";
const char kAutofillManualFallbackAddressEntryActionsHistogram[] =
    "Mobile.ContextMenu.AutofillManualFallbackAddressEntry.Actions";
const char kTabGroupsPanelEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabGroupsPanelEntry.Actions";
const char kSortDriveItemsEntryActionsHistogram[] =
    "Mobile.ContextMenu.SortDriveItemsEntry.Actions";
const char kSelectDriveIdentityEntryActionsHistogram[] =
    "Mobile.ContextMenu.SelectDriveIdentityEntry.Actions";
const char kTabGroupIndicatorEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabGroupIndicatorEntry.Actions";
const char kAutofillManualFallbackPlusAddressEntryActionsHistogram[] =
    "Mobile.ContextMenu.AutofillManualFallbackPlusAddressEntry.Actions";
const char kTabGroupIndicatorNTPEntryActionsHistogram[] =
    "Mobile.ContextMenu.TabGroupIndicatorNTPEntry.Actions";
const char kLastVisitedHistoryEntryActionsHistogram[] =
    "Mobile.ContextMenu.LastVisitedHistoryEntry.Actions";
// LINT.ThenChange(/tools/metrics/histograms/metadata/mobile/histograms.xml)
}  // namespace

void RecordMenuShown(MenuScenarioHistogram scenario) {
  base::UmaHistogramEnumeration(kMenuEntryPointsHistogram, scenario,
                                kMenuScenarioHistogramCount);
}

const char* GetActionsHistogramName(MenuScenarioHistogram scenario) {
  switch (scenario) {
    case kMenuScenarioHistogramHistoryEntry:
      return kHistoryEntryActionsHistogram;
    case kMenuScenarioHistogramBookmarkEntry:
      return kBookmarkEntryActionsHistogram;
    case kMenuScenarioHistogramReadingListEntry:
      return kReadingListEntryActionsHistogram;
    case kMenuScenarioHistogramRecentTabsEntry:
      return kRecentTabsEntryActionsHistogram;
    case kMenuScenarioHistogramRecentTabsHeader:
      return kRecentTabsHeaderActionsHistogram;
    case kMenuScenarioHistogramMostVisitedEntry:
      return kMostVisitedEntryActionsHistogram;
    case kMenuScenarioHistogramBookmarkFolder:
      return kBookmarkFolderActionsHistogram;
    case kMenuScenarioHistogramContextMenuImage:
      return KContextMenuImageActionsHistogram;
    case kMenuScenarioHistogramContextMenuImageLink:
      return KContextMenuImageLinkActionsHistogram;
    case kMenuScenarioHistogramContextMenuLink:
      return KContextMenuLinkActionsHistogram;
    case kMenuScenarioHistogramTabGridEntry:
    case kMenuScenarioHistogramTabGroupGridEntry:
    case kMenuScenarioHistogramThumbStrip:
      return kTabGridActionsHistogram;
    case kMenuScenarioHistogramTabGridAddTo:
      return kTabGridAddToActionsHistogram;
    case kMenuScenarioHistogramTabGridEdit:
      return kTabGridEditActionsHistogram;
    case kMenuScenarioHistogramTabGridSearchResult:
      return kTabGridSearchResultHistogram;
    case kMenuScenarioHistogramToolbarMenu:
      return kToolbarMenuActionsHistogram;
    case kMenuScenarioHistogramOmniboxMostVisitedEntry:
      return kOmniboxMostVisitedEntryActionsHistogram;
    case kMenuScenarioHistogramPinnedTabsEntry:
      return kPinnedTabsEntryActionsHistogram;
    case kMenuScenarioHistogramTabStripEntry:
      return kTabStripEntryActionsHistogram;
    case kMenuScenarioHistogramInactiveTabsEntry:
      return kInactiveTabsEntryActionsHistogram;
    case kMenuScenarioHistogramTabGroupViewMenuEntry:
      return kTabGroupEntryActionsHistogram;
    case kMenuScenarioHistogramTabGroupViewTabEntry:
      return kTabGroupViewEntryActionsHistogram;
    case kMenuScenarioHistogramAutofillManualFallbackAllPasswordsEntry:
      return kAutofillManualFallbackAllPasswordsEntryActionsHistogram;
    case kMenuScenarioHistogramAutofillManualFallbackPasswordEntry:
      return kAutofillManualFallbackPasswordEntryActionsHistogram;
    case kMenuScenarioHistogramAutofillManualFallbackPaymentEntry:
      return kAutofillManualFallbackPaymentEntryActionsHistogram;
    case kMenuScenarioHistogramAutofillManualFallbackAddressEntry:
      return kAutofillManualFallbackAddressEntryActionsHistogram;
    case kMenuScenarioHistogramTabGroupsPanelEntry:
      return kTabGroupsPanelEntryActionsHistogram;
    case kMenuScenarioHistogramSortDriveItemsEntry:
      return kSortDriveItemsEntryActionsHistogram;
    case kMenuScenarioHistogramSelectDriveIdentityEntry:
      return kSelectDriveIdentityEntryActionsHistogram;
    case kMenuScenarioHistogramTabGroupIndicatorEntry:
      return kTabGroupIndicatorEntryActionsHistogram;
    case kMenuScenarioHistogramAutofillManualFallbackPlusAddressEntry:
      return kAutofillManualFallbackPlusAddressEntryActionsHistogram;
    case kMenuScenarioHistogramTabGroupIndicatorNTPEntry:
      return kTabGroupIndicatorNTPEntryActionsHistogram;
    case kMenuScenarioHistogramLastVisitedHistoryEntry:
      return kLastVisitedHistoryEntryActionsHistogram;
    case kMenuScenarioHistogramCount:
      NOTREACHED();
  }
}
