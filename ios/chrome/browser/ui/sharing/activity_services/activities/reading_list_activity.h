// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GURL;
class ReadingListBrowserAgent;

// Activity that triggers the add-to-reading-list service.
@interface ReadingListActivity : UIActivity

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
    readingListBrowserAgent:(ReadingListBrowserAgent*)readingListBrowserAgent;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITIES_READING_LIST_ACTIVITY_H_
