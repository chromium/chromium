// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_BUILDER_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_BUILDER_H_

#include <memory>

@class CRWNavigationItemStorage;

namespace web {

class NavigationItemImpl;

// Class that can serialize and deserialize NavigationItems.
class NavigationItemStorageBuilder {
 public:
  // Returns approximate sizes of the given |navigation_item| without building
  // storage. Only string sizes are added.
  int ItemStoredSize(NavigationItemImpl* navigation_item) const;
  // Creates a serialized NavigationItem from |navigation_Item|.
  CRWNavigationItemStorage* BuildStorage(
      NavigationItemImpl* navigation_item) const;
  // Creates a NavigationItem from |navigation_Item_storage|.
  std::unique_ptr<NavigationItemImpl> BuildNavigationItemImpl(
      CRWNavigationItemStorage* navigation_item_storage) const;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_BUILDER_H_
