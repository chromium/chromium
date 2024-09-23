// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_MODEL_WINDOW_ACTIVITY_HELPERS_H_
#define IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_MODEL_WINDOW_ACTIVITY_HELPERS_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

namespace web {
class WebStateID;
}  // namespace web
struct UrlLoadParams;

// Window activity origins.  Please add new origins at the end, to keep
// numeric values of existing origins, and update kMaxValue.
// If new values are added, they must also be added to the WindowActivityOrigin
// histogram definition in //tools/metrics/histograms/metadata/ios/enums.xml
typedef NS_ENUM(NSInteger, WindowActivityOrigin) {
  WindowActivityUnknownOrigin = 0,
  // The command origin comes outside of chrome.
  WindowActivityExternalOrigin,
  // The command origin comes from restoring a session.
  WindowActivityRestoredOrigin,
  // The command origin comes from the context menu.
  WindowActivityContextMenuOrigin,
  // The command origin comes from the reading list.
  WindowActivityReadingListOrigin,
  // The command origin comes from bookmarks.
  WindowActivityBookmarksOrigin,
  // The command origin comes from history.
  WindowActivityHistoryOrigin,
  // The command origin comes from tools.
  WindowActivityToolsOrigin,
  // The command origin comes from recent tabs.
  WindowActivityRecentTabsOrigin,
  // The command origin comes from the location bar steady view.
  WindowActivityLocationBarSteadyViewOrigin,
  // The command origin comes from the NTP content suggestions.
  WindowActivityContentSuggestionsOrigin,
  // The command origin comes from dragging a tab to create a new window.
  WindowActivityTabDragOrigin,
  // The command origin comes from a keyboard shortcut.
  WindowActivityKeyCommandOrigin,
  // Size of enum.
  kMaxValue = WindowActivityKeyCommandOrigin
};

// Helper functions to create NSUserActivity instances that encode specific
// actions in the browser, and to decode those actions from those activities.

// Create a new activity that opens a new tab, loading `url` with the referrer
// `referrer`. `in_incognito` indicates if the new tab should be incognito.
NSUserActivity* ActivityToLoadURL(WindowActivityOrigin origin,
                                  const GURL& url,
                                  const web::Referrer& referrer,
                                  BOOL in_incognito);

// Create a new activity that opens a new regular (non-incognito) tab, loading
// `url`.
NSUserActivity* ActivityToLoadURL(WindowActivityOrigin origin, const GURL& url);

// Create a new activity that moves a tab either between browsers, or reorders
// within a browser.
NSUserActivity* ActivityToMoveTab(web::WebStateID tab_id, BOOL incognito);

// Returns an activity based on `activity_to_adapt` info, changing its mode to
// follow `incognito`.
NSUserActivity* AdaptUserActivityToIncognito(NSUserActivity* activity_to_adapt,
                                             BOOL incognito);

// true if `activity` is one that indicates a URL load (including loading the
// new tab page in a new tab).
BOOL ActivityIsURLLoad(NSUserActivity* activity);

// true if `activity` is one that indicates a tab move.
BOOL ActivityIsTabMove(NSUserActivity* activity);

// The URLLoadParams needed to perform the load defined in `activity`, if any.
// If `activity` is not a URL load activity, the default UrlLoadParams are
// returned.
UrlLoadParams LoadParamsFromActivity(NSUserActivity* activity);

// Returns the recorded origin for the given activity.
WindowActivityOrigin OriginOfActivity(NSUserActivity* activity);

// Returns the tab identifier from the given activity.
web::WebStateID GetTabIDFromActivity(NSUserActivity* activity);

// Returns `true` if the activity is a tab move activity and has the incognito
// flag set.
BOOL GetIncognitoFromTabMoveActivity(NSUserActivity* activity);

#endif  // IOS_CHROME_BROWSER_WINDOW_ACTIVITIES_MODEL_WINDOW_ACTIVITY_HELPERS_H_
