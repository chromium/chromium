// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/test_native_content_provider.h"

#include <map>

#include "ios/web/public/web_client.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestNativeContentProvider {
  std::map<GURL, id<CRWNativeContent>> _nativeContent;
}

- (void)setController:(id<CRWNativeContent>)controller forURL:(const GURL&)URL {
  _nativeContent[URL] = controller;
}

- (BOOL)hasControllerForURL:(const GURL&)URL {
  return _nativeContent.find(URL) != _nativeContent.end();
}

- (id<CRWNativeContent>)controllerForURL:(const GURL&)URL
                                webState:(web::WebState*)webState {
  DCHECK(web::GetWebClient()->IsAppSpecificURL(URL));
  auto nativeContent = _nativeContent.find(URL);
  return nativeContent == _nativeContent.end() ? nil : nativeContent->second;
}

- (UIEdgeInsets)nativeContentInsetForWebState:(web::WebState*)webState {
  return UIEdgeInsetsZero;
}

@end
