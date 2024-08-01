// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_DISTANT_TABS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_DISTANT_TABS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

@class FakeDistantTab;

// A container that create fake foreign sessions and inject them to the sync
// server used by tests.
@interface DistantTabsAppInterface : NSObject

// Adds a session with the given lis of `tabs`.
+ (void)addSessionToFakeSyncServer:(NSString*)sessionName
                 modifiedTimeDelta:(base::TimeDelta)modifiedTimeDelta
                              tabs:(NSArray<FakeDistantTab*>*)tabs;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_DISTANT_TABS_APP_INTERFACE_H_
