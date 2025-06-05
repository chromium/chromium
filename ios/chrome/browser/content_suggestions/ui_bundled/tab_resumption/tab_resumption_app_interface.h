// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App implementation for tab resumption.
@interface TabResumptionAppInterface : NSObject

// Replaces the shopping service with one that can mock responses.
+ (void)setUpMockShoppingService;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_TAB_RESUMPTION_APP_INTERFACE_H_
