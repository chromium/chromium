// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the Assistant AIM UI.
@protocol AssistantAIMConsumer <NSObject>

// Sets the WebState view to be displayed.
- (void)setWebStateView:(UIView*)webStateView;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_
