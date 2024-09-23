// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_CONSUMER_H_

#import "base/time/time.h"

// Consumer protocol for the PageInfoHistoryMediator to provide the Last Visited
// timestamp to Page Info's view controller.
@protocol PageInfoHistoryConsumer <NSObject>

// Displays the Last Visited Row on Page Info.
- (void)setLastVisitedTimestamp:(base::Time)lastVisited;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_CONSUMER_H_
