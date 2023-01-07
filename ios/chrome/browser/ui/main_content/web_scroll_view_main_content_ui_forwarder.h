// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_WEB_SCROLL_VIEW_MAIN_CONTENT_UI_FORWARDER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_WEB_SCROLL_VIEW_MAIN_CONTENT_UI_FORWARDER_H_

#import <Foundation/Foundation.h>

@class MainContentUIStateUpdater;
class WebStateList;

// Helper object that forwards a CRWWebViewScrollViewProxy events to a
// MainContentUIStateUpdater.
@interface WebScrollViewMainContentUIForwarder : NSObject

// Designated initializer for a forwarder that sends the scroll events from
// `webStateList`'s active WebState's scroll view proxy to `updater`.
- (nullable instancetype)initWithUpdater:
                             (nonnull MainContentUIStateUpdater*)updater
                            webStateList:(nonnull WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Instructs the forwarder to stop observing the WebStateList and the active
// WebState's scroll view proxy.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_WEB_SCROLL_VIEW_MAIN_CONTENT_UI_FORWARDER_H_
