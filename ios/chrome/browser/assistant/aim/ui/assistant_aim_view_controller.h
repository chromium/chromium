// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_AIM_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_AIM_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_consumer.h"

@interface AssistantAIMViewController : UIViewController <AssistantAIMConsumer>

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_AIM_UI_ASSISTANT_AIM_VIEW_CONTROLLER_H_
