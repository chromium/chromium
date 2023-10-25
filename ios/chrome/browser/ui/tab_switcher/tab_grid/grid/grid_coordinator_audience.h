// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COORDINATOR_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COORDINATOR_AUDIENCE_H_

// Audience for the Grid coordinators.
@protocol GridCoordinatorAudience

// Called when the incognito grid was updated.
- (void)incognitoGridDidChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_COORDINATOR_AUDIENCE_H_
