// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/url_test_util.h"

#import "ios/web/navigation/navigation_item_impl.h"

namespace web {

std::u16string GetDisplayTitleForUrl(const GURL& url) {
  return NavigationItemImpl::GetDisplayTitleForURL(url);
}

}  // namespace web
