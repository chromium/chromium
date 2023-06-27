// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_CRW_FAKE_WEB_VIEW_NAVIGATION_PROXY_H_
#define IOS_WEB_TEST_FAKES_CRW_FAKE_WEB_VIEW_NAVIGATION_PROXY_H_

#import <Foundation/Foundation.h>

#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"

@interface CRWFakeWebViewNavigationProxy : NSObject <CRWWebViewNavigationProxy>

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(NSArray<NSString*>*)backListURLs
      forwardListURLs:(NSArray<NSString*>*)forwardListURLs;

@end

#endif  // IOS_WEB_TEST_FAKES_CRW_FAKE_WEB_VIEW_NAVIGATION_PROXY_H_
