// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_MUTATOR_H_

// Consumer protocol for the PageInfoHistoryViewController to ask for an update
// of the Last Visited timestamp to Page Info's Mediator.
@protocol PageInfoHistoryMutator <NSObject>

// Requests for an update of the Last Visited timestamp.
- (void)lastVisitedTimestampNeedsUpdate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HISTORY_MUTATOR_H_
