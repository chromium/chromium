// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_UTIL_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_UTIL_H_

namespace feature_engagement {

class Tracker;

// Sends a new tab event to the feature_engagement::Tracker based on
// `is_incognito`. If `is_incognito` is `true`, then the "Incognito Tab Opened"
// is fired. If `is_incognito` is `false`, then the "New Tab Event" is fired.
void NotifyNewTabEvent(Tracker* tracker, bool is_incognito);

}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_UTIL_H_
