// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_FAKE_DISTANT_TAB_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_FAKE_DISTANT_TAB_H_

#import <Foundation/Foundation.h>

class GURL;

// An NSObject that store properties to create a distantTab, used by the
// DistantTabsAppInterface to populate a distantSession.
@interface FakeDistantTab : NSObject

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* URL;

// Returns an array of `numberOfTabs` FakeDistantTab for the given `serverURL`.
+ (NSArray<FakeDistantTab*>*)createFakeTabsForServerURL:(const GURL&)serverURL
                                           numberOfTabs:
                                               (NSUInteger)numberOfTabs;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TESTS_FAKE_DISTANT_TAB_H_
