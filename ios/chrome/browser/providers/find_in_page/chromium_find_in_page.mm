// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/find_in_page/find_in_page_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

bool IsNativeFindInPageWithSystemFindPanel() {
  return false;
}

bool IsNativeFindInPageWithChromeFindBar() {
  return false;
}

bool IsNativeFindInPageEnabled() {
  return false;
}

id<UITextSearching> GetSearchableObjectForWebState(web::WebState* web_state)
    API_AVAILABLE(ios(16)) {
  // `IsNativeFindInPageWithChromeFindBar` is unconditionally `false` so this
  // should never be reached.
  NOTREACHED();
  return nil;
}

void StartTextSearchInWebState(web::WebState* web_state) {
  // `IsNativeFindInPageWithChromeFindBar` is unconditionally `false` so this
  // should never be reached.
  NOTREACHED();
}

void StopTextSearchInWebState(web::WebState* web_state) {
  // `IsNativeFindInPageWithChromeFindBar` is unconditionally `false` so this
  // should never be reached.
  NOTREACHED();
}

}  // namespace provider
}  // namespace ios
