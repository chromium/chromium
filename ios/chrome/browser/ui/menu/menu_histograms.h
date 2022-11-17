// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_

// Enum representing the existing set of menu scenarios. Current values should
// not be renumbered. Please keep in sync with "IOSMenuScenario" in
// src/tools/metrics/histograms/enums.xml.
enum class MenuScenarioHistogram {
  kBookmarkEntry = 0,
  kBookmarkFolder = 1,
  kReadingListEntry = 2,
  kRecentTabsHeader = 3,
  kRecentTabsEntry = 4,
  kHistoryEntry = 5,
  kMostVisitedEntry = 6,
  kContextMenuImage = 7,
  kContextMenuImageLink = 8,
  kContextMenuLink = 9,
  kTabGridEntry = 10,
  kTabGridAddTo = 11,
  kTabGridEdit = 12,
  kToolbarMenu = 13,
  kTabGridSearchResult = 14,
  kThumbStrip = 15,
  kOmniboxMostVisitedEntry = 16,
  kMaxValue = kOmniboxMostVisitedEntry,
};

// Records a menu shown histogram metric for the `scenario`.
void RecordMenuShown(MenuScenarioHistogram scenario);

// Retrieves a histogram name for the given menu `scenario`'s actions.
const char* GetActionsHistogramName(MenuScenarioHistogram scenario);

#endif  // IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_
