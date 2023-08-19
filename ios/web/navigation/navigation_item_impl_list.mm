// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl_list.h"

#import "ios/web/navigation/navigation_item_impl.h"

namespace web {

ScopedNavigationItemImplList CreateScopedNavigationItemImplList(
    ScopedNavigationItemList scoped_item_list) {
  ScopedNavigationItemImplList list(scoped_item_list.size());
  for (size_t index = 0; index < scoped_item_list.size(); ++index) {
    std::unique_ptr<NavigationItemImpl> scoped_item_impl(
        static_cast<NavigationItemImpl*>(scoped_item_list[index].release()));
    list[index] = std::move(scoped_item_impl);
  }
  return list;
}

NavigationItemList CreateNavigationItemList(
    const ScopedNavigationItemImplList& scoped_item_list) {
  NavigationItemList list(scoped_item_list.size());
  for (size_t index = 0; index < scoped_item_list.size(); ++index) {
    list[index] = scoped_item_list[index].get();
  }
  return list;
}

}  // namespace web
