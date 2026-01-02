// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/gemini/ui/assistant_gemini_consumer.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_consumer.h"

@interface AssistantGeminiViewController
    : UIViewController <AssistantGeminiConsumer>

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_VIEW_CONTROLLER_H_
