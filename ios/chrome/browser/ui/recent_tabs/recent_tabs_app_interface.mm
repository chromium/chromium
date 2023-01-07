// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation RecentTabsAppInterface

+ (void)clearCollapsedListViewSectionStates {
  NSArray<UIWindow*>* windows = [UIApplication sharedApplication].windows;
  for (UIWindow* window in windows) {
    UISceneSession* session = window.windowScene.session;

    NSMutableDictionary* newUserInfo =
        [NSMutableDictionary dictionaryWithDictionary:session.userInfo];
    [newUserInfo removeObjectForKey:kListModelCollapsedKey];
    session.userInfo = newUserInfo;
  }
}

@end
