// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONSUMER_H_

#import <Foundation/Foundation.h>

// The consumer for the Page Action Menu UI to update with model changes.
@protocol PageActionMenuConsumer <NSObject>

// Notifies the consumer that the page load status has changed.
- (void)pageLoadStatusChanged;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONSUMER_H_
