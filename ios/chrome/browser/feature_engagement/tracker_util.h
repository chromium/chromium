// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_UTIL_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_UTIL_H_

class ChromeBrowserState;
@class OpenNewTabCommand;

namespace feature_engagement {
// Sends a new tab event to the feature_engagement::Tracker based on
// |isIncognito|. If |isIncognito| is |true|, then the "Incognito Tab Opened"
// is fired. If |isIncognito| is |false|, then the "New Tab Event" is fired.
void NotifyNewTabEvent(ChromeBrowserState* browserState, bool isIncognito);

// Sends a new tab event to the feature_engagement::Tracker based on
// |command.incognito| and |command.userInitiated|. If |command.userInitiated|
// is |false|, then no event is fired. If |command.userInitiated| is |true|,
// then one of the new tab events is fired. If |command.incognito| is |true|,
// then the "Incognito Tab Opened" event is fired, and if |command.incognito| is
// |false|, then the "New Tab Opened" event is fired.
void NotifyNewTabEventForCommand(ChromeBrowserState* browserState,
                                 OpenNewTabCommand* command);
}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_UTIL_H_
