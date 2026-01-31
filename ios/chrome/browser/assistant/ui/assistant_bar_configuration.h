// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/assistant/ui/assistant_bar_item.h"

@class AssistantBarItem;

// Configuration for the assistant bar.
@interface AssistantBarConfiguration : NSObject

@property(nonatomic, copy) NSString* title;

@property(nonatomic, copy) NSArray<AssistantBarItem*>* leadingButtons;
@property(nonatomic, copy) NSArray<AssistantBarItem*>* trailingButtons;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_UI_ASSISTANT_BAR_CONFIGURATION_H_
