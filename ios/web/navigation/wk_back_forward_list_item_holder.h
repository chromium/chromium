// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_
#define IOS_WEB_NAVIGATION_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_

#import <WebKit/WebKit.h>

#include "base/supports_user_data.h"

namespace web {

class NavigationItem;

// This class is a wrapper for information needed to implement native
// WKWebView navigation. WKBackForwardListItemHolder is attached to
// NavigationItem via the SupportsUserData interface and holds the corresponding
// WKBackForwardListItem, as well as any state that is inaccessible later and
// thus needs to be preserved (e.g., WKNavigationType, MIME type).
class WKBackForwardListItemHolder : public base::SupportsUserData::Data {
 public:
  WKBackForwardListItemHolder(const WKBackForwardListItemHolder&) = delete;
  WKBackForwardListItemHolder& operator=(const WKBackForwardListItemHolder&) =
      delete;

  ~WKBackForwardListItemHolder() override;

  // Returns the WKBackForwardListItemHolder for the NavigationItem `item`.
  // Lazily attaches one if it does not exist. `item` cannot be null.
  static web::WKBackForwardListItemHolder* FromNavigationItem(
      NavigationItem* item);

  // Accessors for `item_`. Use these to get/set the association between a
  // NavigationItem and a WKBackForwardListItem. Note that
  // `back_forward_list_item` may return nil (f.e. when the
  // parent WKBackForwardList is deallocated).
  WKBackForwardListItem* back_forward_list_item() const { return item_; }
  void set_back_forward_list_item(WKBackForwardListItem* item) { item_ = item; }

  // Accessors for `navigation_type_`. Use these to get/set the association
  // between a NavigationItem and a WKNavigationType.
  WKNavigationType navigation_type() const { return navigation_type_; }
  void set_navigation_type(WKNavigationType type) { navigation_type_ = type; }

  // Gets/sets HTTP request method for this item.
  NSString* http_method() const { return http_method_; }
  void set_http_method(NSString* http_method) {
    http_method_ = [http_method copy];
  }

  // Gets/sets the MIME type of the page corresponding to this item.
  NSString* mime_type() const { return mime_type_; }
  void set_mime_type(NSString* mime_type) { mime_type_ = [mime_type copy]; }

 private:
  WKBackForwardListItemHolder();

  // Weak pointer to a WKBackForwardListItem. Becomes nil if the parent
  // WKBackForwardList is deallocated.
  __weak WKBackForwardListItem* item_ = nil;

  // The navigation type for the associated NavigationItem.
  WKNavigationType navigation_type_;

  // HTTP request method.
  NSString* http_method_;

  // The MIME type of the page content. Default to HTML, to handle same-
  // document navigations that don't trigger any WebKit navigation callbacks.
  // Initial navigations to non-HTML content always trigger navigation
  // callbacks, which will lead to the correct value being stored here.
  NSString* mime_type_ = @"text/html";
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_BACK_FORWARD_LIST_ITEM_HOLDER_H_
