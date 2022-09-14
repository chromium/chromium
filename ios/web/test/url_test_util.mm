// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/url_test_util.h"

#import "ios/web/navigation/navigation_item_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

std::u16string GetDisplayTitleForUrl(const GURL& url) {
  return NavigationItemImpl::GetDisplayTitleForURL(url);
}

}  // namespace web
