// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_PRIVATE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"

@interface SetUpListItemView (Private)

// Handles a tap event from the tap gesture recognizer.
- (void)handleTap:(UITapGestureRecognizer*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_ITEM_VIEW_PRIVATE_H
