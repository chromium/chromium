// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class SetUpListItemViewData;
@protocol SetUpListTapDelegate;

// View controller to show the entire list of Set Up List elements
@interface SetUpListShowMoreViewController : UIViewController

- (instancetype)initWithItems:(NSArray<SetUpListItemViewData*>*)items
                  tapDelegate:(id<SetUpListTapDelegate>)tapDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_SHOW_MORE_VIEW_CONTROLLER_H_
