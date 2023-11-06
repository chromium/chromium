// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_UTILS_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_UTILS_H_

class Browser;

// Move tabs from the active browser's web state list to the inactive browser's
// web state list. Each of the active tabs is added at the end of the inactive
// tabs list. For example, if your active list have [D, E, F] and your inactive
// [A, B, C] and D and E are finally inactive, the final state will be [F] for
// the active and [A, B, C, D, E] for the inactive. Pinned tabs are ignored and
// will always stay active. Inactive tabs feature should *not* be disabled.
void MoveTabsFromActiveToInactive(Browser* active_browser,
                                  Browser* inactive_browser);

// Move tabs, depending on the threshold condition, from the inactive browser's
// web state list to the active browser's web state list. Each of the inactive
// tabs is added at the beginning of the active tabs list, right after the
// pinned tabs. For example, if your inactive list have [A, B, C] and your
// active [D, E, F] and B and C are finally active, the final state will be [A]
// for the inactive and [B, C, D, E, F] for the active. Inactive tabs feature
// should *not* be disabled.
void MoveTabsFromInactiveToActive(Browser* inactive_browser,
                                  Browser* active_browser);

// Move all tabs from the inactive browser's web state list to the active
// browser's web state list. Each of the inactive tabs is added at the beginning
// of the active tabs list, right after the pinned tabs. Inactive tabs feature
// should be disabled.
void RestoreAllInactiveTabs(Browser* inactive_browser, Browser* active_browser);

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_INACTIVE_TABS_UTILS_H_
