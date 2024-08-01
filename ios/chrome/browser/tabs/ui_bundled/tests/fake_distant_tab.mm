// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tests/fake_distant_tab.h"

#import "base/strings/sys_string_conversions.h"
#import "url/gurl.h"

#pragma mark - FakeDistantTab

@implementation FakeDistantTab

+ (NSArray<FakeDistantTab*>*)createFakeTabsForServerURL:(const GURL&)serverURL
                                           numberOfTabs:
                                               (NSUInteger)numberOfTabs {
  NSMutableArray<FakeDistantTab*>* fakeTabs = [NSMutableArray array];
  for (int i = numberOfTabs - 1; i >= 0; --i) {
    FakeDistantTab* tab = [[FakeDistantTab alloc] init];
    const GURL& tabURL =
        serverURL.Resolve("/distant_tab_" + base::NumberToString(i) + ".html");
    NSString* stringTabURL = base::SysUTF8ToNSString(tabURL.spec());

    tab.title = [NSString stringWithFormat:@"Tab %d", i];
    tab.URL = stringTabURL;
    [fakeTabs addObject:tab];
  }
  return fakeTabs;
}

@end
