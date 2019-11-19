// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_util.h"

#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NavigationItemImpl* GetItemWithUniqueID(
    NavigationManagerImpl* navigation_manager,
    NavigationContextImpl* context) {
  if (context->GetItem())
    return context->GetItem();

  int unique_id = context->GetNavigationItemUniqueID();
  NavigationItemImpl* transient_item =
      navigation_manager->GetTransientItemImpl();
  if (transient_item && transient_item->GetUniqueID() == unique_id)
    return transient_item;

  NavigationItemImpl* pending_item =
      navigation_manager->GetPendingItemInCurrentOrRestoredSession();
  if (pending_item && pending_item->GetUniqueID() == unique_id)
    return pending_item;

  return GetCommittedItemWithUniqueID(navigation_manager, unique_id);
}

NavigationItemImpl* GetCommittedItemWithUniqueID(
    NavigationManagerImpl* navigation_manager,
    int unique_id) {
  int index = GetCommittedItemIndexWithUniqueID(navigation_manager, unique_id);
  return index != -1 ? navigation_manager->GetNavigationItemImplAtIndex(index)
                     : nullptr;
}

int GetCommittedItemIndexWithUniqueID(NavigationManager* navigation_manager,
                                      int unique_id) {
  for (int i = 0; i < navigation_manager->GetItemCount(); i++) {
    web::NavigationItem* item = navigation_manager->GetItemAtIndex(i);
    if (item->GetUniqueID() == unique_id) {
      return i;
    }
  }
  return -1;
}

}  // namespace web
