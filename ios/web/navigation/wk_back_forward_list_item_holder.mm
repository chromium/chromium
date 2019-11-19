// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_back_forward_list_item_holder.h"

#include "base/memory/ptr_util.h"
#import "ios/web/public/navigation/navigation_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
// Private key used for safe conversion of base::SupportsUserData to
// web::WKBackForwardListItemHolder in
// web::WKBackForwardListItemHolder::FromNavigationItem.
const char kBackForwardListItemIdentifierKey[] =
    "BackForwardListItemIdentifierKey";
}

WKBackForwardListItemHolder::WKBackForwardListItemHolder()
    : navigation_type_(WKNavigationTypeOther) {}

WKBackForwardListItemHolder::~WKBackForwardListItemHolder() {}

// static
WKBackForwardListItemHolder* WKBackForwardListItemHolder::FromNavigationItem(
    web::NavigationItem* item) {
  DCHECK(item);
  base::SupportsUserData::Data* user_data =
      item->GetUserData(kBackForwardListItemIdentifierKey);
  if (!user_data) {
    user_data = new WKBackForwardListItemHolder();
    item->SetUserData(kBackForwardListItemIdentifierKey,
                      base::WrapUnique(user_data));
  }
  return static_cast<WKBackForwardListItemHolder*>(user_data);
}

}  // namespace web
