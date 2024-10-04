// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_

// Enum representing the existing set of menu scenarios. Current values should
// not be renumbered. Please keep in sync with "IOSMenuScenario" in
// src/tools/metrics/histograms/metadata/mobile/enums.xml.
// LINT.IfChange
enum MenuScenarioHistogram {
  kMenuScenarioHistogramBookmarkEntry = 0,
  kMenuScenarioHistogramBookmarkFolder = 1,
  kMenuScenarioHistogramReadingListEntry = 2,
  kMenuScenarioHistogramRecentTabsHeader = 3,
  kMenuScenarioHistogramRecentTabsEntry = 4,
  kMenuScenarioHistogramHistoryEntry = 5,
  kMenuScenarioHistogramMostVisitedEntry = 6,
  kMenuScenarioHistogramContextMenuImage = 7,
  kMenuScenarioHistogramContextMenuImageLink = 8,
  kMenuScenarioHistogramContextMenuLink = 9,
  kMenuScenarioHistogramTabGridEntry = 10,
  kMenuScenarioHistogramTabGridAddTo = 11,
  kMenuScenarioHistogramTabGridEdit = 12,
  kMenuScenarioHistogramToolbarMenu = 13,
  kMenuScenarioHistogramTabGridSearchResult = 14,
  kMenuScenarioHistogramThumbStrip = 15,
  kMenuScenarioHistogramOmniboxMostVisitedEntry = 16,
  kMenuScenarioHistogramPinnedTabsEntry = 17,
  kMenuScenarioHistogramTabStripEntry = 18,
  kMenuScenarioHistogramInactiveTabsEntry = 19,
  kMenuScenarioHistogramTabGroupGridEntry = 20,
  kMenuScenarioHistogramTabGroupViewMenuEntry = 21,
  kMenuScenarioHistogramTabGroupViewTabEntry = 22,
  kMenuScenarioHistogramAutofillManualFallbackAllPasswordsEntry = 23,
  kMenuScenarioHistogramAutofillManualFallbackPasswordEntry = 24,
  kMenuScenarioHistogramAutofillManualFallbackPaymentEntry = 25,
  kMenuScenarioHistogramAutofillManualFallbackAddressEntry = 26,
  kMenuScenarioHistogramTabGroupsPanelEntry = 27,
  kMenuScenarioHistogramSortDriveItemsEntry = 28,
  kMenuScenarioHistogramSelectDriveIdentityEntry = 29,
  kMenuScenarioHistogramTabGroupIndicatorEntry = 30,
  kMenuScenarioHistogramAutofillManualFallbackPlusAddressEntry = 31,
  kMenuScenarioHistogramTabGroupIndicatorNTPEntry = 32,
  kMenuScenarioHistogramLastVisitedHistoryEntry = 33,
  kMenuScenarioHistogramCount,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/mobile/enums.xml)

// Records a menu shown histogram metric for the `scenario`.
void RecordMenuShown(enum MenuScenarioHistogram scenario);

// Retrieves a histogram name for the given menu `scenario`'s actions.
const char* GetActionsHistogramName(enum MenuScenarioHistogram scenario);

#endif  // IOS_CHROME_BROWSER_UI_MENU_MENU_HISTOGRAMS_H_
