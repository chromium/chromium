// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol BrowserContainerConsumer <NSObject>

// Whether the content view should be blocked.  When set to YES, the content
// area is blocked.  Overlay UI shown in OverlayModality::kWebContentArea remain
// visible when `contentBlocked` is YES.
- (void)setContentBlocked:(BOOL)contentBlocked;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_CONTAINER_BROWSER_CONTAINER_CONSUMER_H_
