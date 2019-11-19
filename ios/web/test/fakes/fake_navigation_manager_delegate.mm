// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_navigation_manager_delegate.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void FakeNavigationManagerDelegate::ClearTransientContent() {}
void FakeNavigationManagerDelegate::ClearDialogs() {}
void FakeNavigationManagerDelegate::RecordPageStateInNavigationItem() {}
void FakeNavigationManagerDelegate::OnGoToIndexSameDocumentNavigation(
    NavigationInitiationType type,
    bool has_user_gesture) {}
void FakeNavigationManagerDelegate::WillChangeUserAgentType() {}
void FakeNavigationManagerDelegate::LoadCurrentItem(
    NavigationInitiationType type) {}
void FakeNavigationManagerDelegate::LoadIfNecessary() {}
void FakeNavigationManagerDelegate::Reload() {}
void FakeNavigationManagerDelegate::OnNavigationItemsPruned(
    size_t pruned_item_count) {}
void FakeNavigationManagerDelegate::OnNavigationItemCommitted(
    NavigationItem* item) {}
WebState* FakeNavigationManagerDelegate::GetWebState() {
  return web_state_;
}
id<CRWWebViewNavigationProxy>
FakeNavigationManagerDelegate::GetWebViewNavigationProxy() const {
  return test_web_view_;
}
void FakeNavigationManagerDelegate::GoToBackForwardListItem(
    WKBackForwardListItem* wk_item,
    NavigationItem* item,
    NavigationInitiationType type,
    bool has_user_gesture) {}
void FakeNavigationManagerDelegate::RemoveWebView() {}

NavigationItemImpl* FakeNavigationManagerDelegate::GetPendingItem() {
  return nullptr;
}

void FakeNavigationManagerDelegate::SetWebViewNavigationProxy(id web_view) {
  test_web_view_ = web_view;
}

void FakeNavigationManagerDelegate::SetWebState(WebState* web_state) {
  web_state_ = web_state;
}

}  // namespace web
