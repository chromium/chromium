// Copyright 2017 The Chromium Authors
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
  // Creates a serialized NavigationItem from `navigation_item`.
  static CRWNavigationItemStorage* BuildStorage(
      const NavigationItemImpl& navigation_item);

  // Creates a NavigationItem from `navigation_item_storage`.
  static std::unique_ptr<NavigationItemImpl> BuildNavigationItemImpl(
      CRWNavigationItemStorage* navigation_item_storage);

  NavigationItemStorageBuilder() = delete;
  ~NavigationItemStorageBuilder() = delete;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_STORAGE_BUILDER_H_
