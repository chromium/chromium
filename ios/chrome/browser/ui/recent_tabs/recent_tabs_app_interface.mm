// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation RecentTabsAppInterface

+ (void)clearCollapsedListViewSectionStates {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UISceneSession* session = scene.session;
    NSMutableDictionary* newUserInfo =
        [NSMutableDictionary dictionaryWithDictionary:session.userInfo];
    [newUserInfo removeObjectForKey:kListModelCollapsedKey];
    session.userInfo = newUserInfo;
  }
}

@end
