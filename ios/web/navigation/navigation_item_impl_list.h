// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_LIST_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_LIST_H_

#import "ios/web/public/deprecated/navigation_item_list.h"

namespace web {

class NavigationItemImpl;

// Convenience typedef for a list of raw NavigationItem pointers.
typedef std::vector<NavigationItemImpl*> NavigationItemImplList;

// Convenience typedef for a list of scoped NavigationItem pointers.
typedef std::vector<std::unique_ptr<NavigationItemImpl>>
    ScopedNavigationItemImplList;

// Creates a ScopedNavigationItemImplList from |scoped_item_list|.  Ownership
// of the NavigationItems in |scoped_item_list| is transferred to the returned
// value.
ScopedNavigationItemImplList CreateScopedNavigationItemImplList(
    ScopedNavigationItemList scoped_item_list);

// Creates a NavigationItemList from |scoped_item_list|.
NavigationItemList CreateNavigationItemList(
    const ScopedNavigationItemImplList& scoped_item_list);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_LIST_H_
