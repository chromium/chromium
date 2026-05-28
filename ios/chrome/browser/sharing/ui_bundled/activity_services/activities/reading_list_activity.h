// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/chrome_activity.h"

class GURL;
class ReadingListBrowserAgent;

// Activity that triggers the add-to-reading-list service.
@interface ReadingListActivity : ChromeActivity

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
    readingListBrowserAgent:(ReadingListBrowserAgent*)readingListBrowserAgent;

@end

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_
