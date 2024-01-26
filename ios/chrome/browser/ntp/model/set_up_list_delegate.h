// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_DELEGATE_H_

// Protocol that SetUpList uses to tell its delegate of events.
@protocol SetUpListDelegate

// Indicates that a SetUpList item has been completed.
- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed;

@end

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_DELEGATE_H_
