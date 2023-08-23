// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/test_redirect_observer.h"

#import "base/containers/contains.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace web {

#pragma mark - TestRedirectObserver

TestRedirectObserver::TestRedirectObserver(WebState* web_state) {
  web_state->AddObserver(this);
}

TestRedirectObserver::~TestRedirectObserver() {}

void TestRedirectObserver::BeginObservingRedirectsForUrl(const GURL& url) {
  expected_urls_.insert(url);
}

GURL TestRedirectObserver::GetFinalUrlForUrl(const GURL& url) {
  for (auto redirect_chain_for_item : redirect_chains_) {
    RedirectChain redirect_chain = redirect_chain_for_item.second;
    if (redirect_chain.original_url == url)
      return redirect_chain.final_url;
  }
  // If load for `url` did not occur after BeginObservingRedirectsForUrl() is
  // called, there will be no final redirected URL.
  return GURL();
}

void TestRedirectObserver::DidStartNavigation(web::WebState* web_state,
                                              NavigationContext* context) {
  GURL url = context->GetUrl();
  NavigationItem* item = web_state->GetNavigationManager()->GetVisibleItem();
  DCHECK(item);
  if (base::Contains(redirect_chains_, item)) {
    // If the redirect chain for the pending NavigationItem is already being
    // tracked, add the new URL to the end of the chain.
    redirect_chains_[item].final_url = url;
  } else if (base::Contains(expected_urls_, url)) {
    // If a load has begun for an expected URL, begin observing the redirect
    // chain for that NavigationItem.
    expected_urls_.erase(url);
    RedirectChain redirect_chain;
    redirect_chain.original_url = url;
    redirect_chain.final_url = url;
    redirect_chains_[item] = redirect_chain;
  }
}

void TestRedirectObserver::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

WEB_STATE_USER_DATA_KEY_IMPL(TestRedirectObserver)

}  // namespace web
