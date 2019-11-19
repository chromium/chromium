// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/navigation_item_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NavigationItemList CreateRawNavigationItemList(
    const ScopedNavigationItemList& scoped_list) {
  NavigationItemList list(scoped_list.size());
  for (size_t index = 0; index < scoped_list.size(); ++index)
    list[index] = scoped_list[index].get();
  return list;
}

}  // namespace web
