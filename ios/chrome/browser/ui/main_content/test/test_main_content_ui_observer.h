// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_TEST_MAIN_CONTENT_UI_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_TEST_MAIN_CONTENT_UI_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

// An object that observes the main content scroll view's y offset, scrolling,
// and dragging state.
@interface TestMainContentUIObserver : NSObject<ChromeBroadcastObserver>

// The broadcaster.  Setting will start observing broadcast values from the
// broadcaster.
@property(nonatomic, strong) ChromeBroadcaster* broadcaster;
// The broadcasted UI state observed by this object.
@property(nonatomic, readonly) CGFloat yOffset;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_TEST_TEST_MAIN_CONTENT_UI_OBSERVER_H_
