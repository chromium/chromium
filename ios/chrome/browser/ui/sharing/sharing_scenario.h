// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARING_SCENARIO_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARING_SCENARIO_H_

// Enum that contains the list of sharing scenarios. Current values should not
// be renumbered. Please keep in sync with "IOSActivityScenario" in
// src/tools/metrics/histograms/enums.xml.
// LINT.IfChange
enum class SharingScenario {
  TabShareButton = 0,
  QRCodeImage = 1,
  HistoryEntry = 2,
  ReadingListEntry = 3,
  BookmarkEntry = 4,
  MostVisitedEntry = 5,
  RecentTabsEntry = 6,
  SharedHighlight = 7,
  TabGridItem = 8,
  TabGridSelectionMode = 9,
  ShareChrome = 10,
  OmniboxMostVisitedEntry = 11,
  TabStripItem = 12,
  ShareInWebContextMenu = 13,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = ShareInWebContextMenu
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/mobile/enums.xml)

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARING_SCENARIO_H_
