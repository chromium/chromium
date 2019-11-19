// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_storage_builder.h"

#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

CRWNavigationItemStorage* NavigationItemStorageBuilder::BuildStorage(
    NavigationItemImpl* navigation_item) const {
  DCHECK(navigation_item);
  CRWNavigationItemStorage* storage = [[CRWNavigationItemStorage alloc] init];
  storage.virtualURL = navigation_item->GetVirtualURL();
  storage.referrer = navigation_item->GetReferrer();
  storage.timestamp = navigation_item->GetTimestamp();
  storage.title = navigation_item->GetTitle();
  storage.displayState = navigation_item->GetPageDisplayState();
  storage.shouldSkipRepostFormConfirmation =
      navigation_item->ShouldSkipRepostFormConfirmation();
  storage.userAgentType = navigation_item->GetUserAgentType();
  storage.POSTData = navigation_item->GetPostData();
  storage.HTTPRequestHeaders = navigation_item->GetHttpRequestHeaders();
  return storage;
}

std::unique_ptr<NavigationItemImpl>
NavigationItemStorageBuilder::BuildNavigationItemImpl(
    CRWNavigationItemStorage* navigation_item_storage) const {
  std::unique_ptr<NavigationItemImpl> item(new web::NavigationItemImpl());
  // While the virtual URL is persisted, we still need the original request URL
  // and the non-virtual URL to be set upon NavigationItem creation.  Since
  // GetVirtualURL() returns |url_| for the non-overridden case, this will also
  // update the virtual URL reported by this object.
  item->original_request_url_ = navigation_item_storage.virtualURL;
  item->SetURL(navigation_item_storage.virtualURL);
  item->referrer_ = navigation_item_storage.referrer;
  item->timestamp_ = navigation_item_storage.timestamp;
  item->title_ = navigation_item_storage.title;
  item->page_display_state_ = navigation_item_storage.displayState;
  item->should_skip_repost_form_confirmation_ =
      navigation_item_storage.shouldSkipRepostFormConfirmation;
  // Use reload transition type to avoid incorrect increase for typed count.
  item->transition_type_ = ui::PAGE_TRANSITION_RELOAD;
  item->user_agent_type_ = navigation_item_storage.userAgentType;
  item->post_data_ = navigation_item_storage.POSTData;
  item->http_request_headers_ =
      [navigation_item_storage.HTTPRequestHeaders mutableCopy];
  return item;
}

}  // namespace web
