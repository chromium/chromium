// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the Assistant Sheet.
@protocol AssistantSheetConsumer <NSObject>

// Sets the child view controller to be displayed in the sheet content area.
- (void)setChildViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_SHEET_CONSUMER_H_
