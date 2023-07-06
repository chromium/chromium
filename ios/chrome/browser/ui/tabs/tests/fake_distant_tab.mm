// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tests/fake_distant_tab.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - FakeDistantTab

@implementation FakeDistantTab

+ (NSArray<FakeDistantTab*>*)createFakeTabsForServerURL:(const GURL&)serverURL
                                           numberOfTabs:
                                               (NSUInteger)numberOfTabs {
  NSMutableArray<FakeDistantTab*>* fakeTabs = [NSMutableArray array];
  for (NSUInteger i = 0; i < numberOfTabs; ++i) {
    FakeDistantTab* tab = [[FakeDistantTab alloc] init];
    const GURL& tabURL =
        serverURL.Resolve("/distant_tab_" + base::NumberToString(i) + ".html");
    NSString* stringTabURL = base::SysUTF8ToNSString(tabURL.spec());

    tab.title = [NSString stringWithFormat:@"Tab %ld", i];
    tab.URL = stringTabURL;
    [fakeTabs addObject:tab];
  }
  return fakeTabs;
}

@end
