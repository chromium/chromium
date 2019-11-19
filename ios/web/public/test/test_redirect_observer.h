// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_TEST_REDIRECT_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_TEST_REDIRECT_OBSERVER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {

class NavigationItem;

// A utility class that is used to track redirects during tests to enable URL
// verification for redirected page loads.
class TestRedirectObserver
    : public web::WebStateObserver,
      public web::WebStateUserData<TestRedirectObserver> {
 public:
  // Notifies the observer that |url| is about to be loaded by the associated
  // WebState, triggering the TestRedirectObserver to start observing redirects.
  void BeginObservingRedirectsForUrl(const GURL& url);

  // Returns the final url in the redirect chain that began with |url|.
  GURL GetFinalUrlForUrl(const GURL& url);

 private:
  friend class web::WebStateUserData<TestRedirectObserver>;

  TestRedirectObserver(WebState* web_state);
  ~TestRedirectObserver() final;

  // WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // RedirectChains store the original and final redirect URLs for a given page
  // load.
  typedef struct RedirectChain {
    GURL original_url;
    GURL final_url;
  } RedirectChain;

  // Stores redirect chains with their corresponding NavigationItems.
  std::map<NavigationItem*, RedirectChain> redirect_chains_;
  // Stores URLs passed into |BeginObservingRedirectsForUrl()|.  Once a
  // provisional load has begin for a URL contained in this set, the URL will
  // be removed and the redirect chain originating from that URL will be stored
  // in |redirect_chains_|.
  std::set<GURL> expected_urls_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(TestRedirectObserver);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_TEST_REDIRECT_OBSERVER_H_
