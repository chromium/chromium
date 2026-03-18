// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TAB_ACTIVITY_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TAB_ACTIVITY_UTILS_H_

#import <Foundation/Foundation.h>

@class NSUserActivity;
class Browser;
class ProfileIOS;

// TODO(crbug.com/491704371): Revisit this implementation following the
// investigation into making Drag and Drop independent from NSUserActivity.

// Returns YES if the activity points to a valid tab in any browser of the same
// type (incognito or regular) for the given profile.
BOOL IsTabActivityValid(NSUserActivity* activity, ProfileIOS* profile);

// Handles a tab move activity by moving the tab to the given browser.
void HandleTabMoveActivity(NSUserActivity* activity, Browser* browser);

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_TAB_ACTIVITY_UTILS_H_
