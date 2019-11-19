// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_NAVIGATION_ITEM_LIST_H_
#define IOS_WEB_PUBLIC_DEPRECATED_NAVIGATION_ITEM_LIST_H_

#include <memory>
#include <vector>

namespace web {

class NavigationItem;

// Convenience typedef for a list of raw NavigationItem pointers.
// TODO(crbug.com/689321): Clean up these typedefs.
typedef std::vector<NavigationItem*> NavigationItemList;

// Convenience typedef for a list of scoped NavigationItem pointers.
typedef std::vector<std::unique_ptr<NavigationItem>> ScopedNavigationItemList;

// Returns a NavigationItemList populated with raw pointer values from
// |scoped_list|.
NavigationItemList CreateRawNavigationItemList(
    const ScopedNavigationItemList& scoped_list);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DEPRECATED_NAVIGATION_ITEM_LIST_H_
