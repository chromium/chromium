// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ADDITIONAL_TEXT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ADDITIONAL_TEXT_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol OmniboxAdditionalTextConsumer <NSObject>

// Notifies the consumer to update the additional text. Set to nil to remove
// additional text.
- (void)updateAdditionalText:(NSString*)additionalText;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ADDITIONAL_TEXT_CONSUMER_H_
