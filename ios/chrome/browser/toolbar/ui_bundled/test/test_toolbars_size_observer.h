// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TEST_TOOLBARS_SIZE_OBSERVER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TEST_TOOLBARS_SIZE_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"

// An object that observes the toolbar's UI state.
@interface TestToolbarsSizeObserver : NSObject <ChromeBroadcastObserver>

// The broadcaster.  Setting will start observing broadcast values from the
// broadcaster.
@property(nonatomic, strong) ChromeBroadcaster* broadcaster;
// The broadcasted UI state observed by this object.
@property(nonatomic, readonly) CGFloat collapsedTopToolbarHeight;
@property(nonatomic, readonly) CGFloat expandedTopToolbarHeight;
@property(nonatomic, readonly) CGFloat expandedBottomToolbarHeight;
@property(nonatomic, readonly) CGFloat collapsedBottomToolbarHeight;

@end
#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TEST_TEST_TOOLBARS_SIZE_OBSERVER_H_
