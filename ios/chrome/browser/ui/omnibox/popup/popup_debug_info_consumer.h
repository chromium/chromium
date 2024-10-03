// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_CONSUMER_H_

#import <UIKit/UIKit.h>

/// An abstract consumer of omnibox popup debug info.
@protocol PopupDebugInfoConsumer <NSObject>

/// Gives the consumer a new variation IDs string to display.
- (void)setVariationIDString:(NSString*)string;

/// Removes all objects from the debug info.
- (void)removeAllObjects;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_DEBUG_INFO_CONSUMER_H_
