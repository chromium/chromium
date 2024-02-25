// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_TEST_FAKES_FAKE_NAVIGATION_MANAGER_DELEGATE_H_

#import "ios/web/navigation/navigation_manager_delegate.h"

#import "base/memory/raw_ptr.h"

@protocol CRWWebViewNavigationProxy;

namespace web {

class FakeNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void ClearDialogs() override;
  void RecordPageStateInNavigationItem() override;
  void LoadCurrentItem(NavigationInitiationType type) override;
  void LoadIfNecessary() override;
  void Reload() override;
  void OnNavigationItemCommitted(NavigationItem* item) override;
  WebState* GetWebState() override;
  void SetWebStateUserAgent(UserAgentType user_agent_type) override;
  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override;
  void GoToBackForwardListItem(WKBackForwardListItem* wk_item,
                               NavigationItem* item,
                               NavigationInitiationType type,
                               bool has_user_gesture) override;
  void RemoveWebView() override;
  NavigationItemImpl* GetPendingItem() override;
  GURL GetCurrentURL() const override;

  // Setters for tests to inject dependencies.
  void SetWebViewNavigationProxy(id test_web_view);
  void SetWebState(WebState*);

 private:
  id test_web_view_;
  raw_ptr<WebState> web_state_ = nullptr;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_NAVIGATION_MANAGER_DELEGATE_H_
