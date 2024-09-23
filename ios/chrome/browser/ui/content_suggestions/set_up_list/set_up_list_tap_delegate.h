// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_TAP_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_TAP_DELEGATE_H_

enum class SetUpListItemType;

// A delegate protocol to handler user events for the SetUpList
@protocol SetUpListTapDelegate <NSObject>

// Called when a Set Up List item is selected by the user.
- (void)didSelectSetUpListItem:(SetUpListItemType)type;

// Called when the view presented from the "See More" button in the Set Up List
// multi-row Magic Stack module should be dismissed.
- (void)dismissSeeMoreViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_TAP_DELEGATE_H_
