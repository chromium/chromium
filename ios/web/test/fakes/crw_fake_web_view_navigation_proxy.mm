// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_web_view_navigation_proxy.h"

#import "ios/web/test/fakes/crw_fake_back_forward_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWebViewNavigationProxy {
  NSURL* _URL;
  CRWFakeBackForwardList* _backForwardList;
}

- (instancetype)init {
  if ((self = [super init])) {
    _backForwardList = [[CRWFakeBackForwardList alloc] init];
  }
  return self;
}

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(NSArray<NSString*>*)backListURLs
      forwardListURLs:(NSArray<NSString*>*)forwardListURLs {
  [_backForwardList setCurrentURL:currentItemURL
                     backListURLs:backListURLs
                  forwardListURLs:forwardListURLs];
  _URL = [NSURL URLWithString:currentItemURL];
}

- (WKBackForwardList*)backForwardList {
  return (id)_backForwardList;
}

- (NSURL*)URL {
  return _URL;
}

@end
