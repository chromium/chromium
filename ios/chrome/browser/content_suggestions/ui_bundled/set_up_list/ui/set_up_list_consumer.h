// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_UI_SET_UP_LIST_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_UI_SET_UP_LIST_CONSUMER_H_

#import "base/ios/block_types.h"

@class SetUpListItem;

// Interface for listening to events occurring in SetUpListMediator.
@protocol SetUpListConsumer

@optional
// Indicates that a SetUpList task has been completed, and whether that resulted
// in all tasks being `completed`. Calls the `completion` block when the
// animation is finished.
- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed
                      completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_UI_SET_UP_LIST_CONSUMER_H_
