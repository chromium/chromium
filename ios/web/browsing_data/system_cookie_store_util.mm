// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browsing_data/system_cookie_store_util.h"

#import <WebKit/WebKit.h>

#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_store.h"
#import "ios/web/net/cookies/wk_cookie_util.h"
#import "ios/web/net/cookies/wk_http_system_cookie_store.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

// Concrete implementation of SystemCookieStoreHandle.
class SystemCookieStoreHandleImpl : public SystemCookieStoreHandle {
 public:
  SystemCookieStoreHandleImpl(WKWebViewConfigurationProvider& provider)
      : cookie_store_([[CRWWKHTTPCookieStore alloc] init]) {
    cookie_store_.websiteDataStore = provider.GetWebsiteDataStore();

    // Using base::Unretained(this) is safe as the callback will not be
    // called after the subscription has been destroyed and it is owned
    // by the current object.
    subscription_ =
        provider.RegisterWebSiteDataStoreUpdatedCallback(base::BindRepeating(
            &SystemCookieStoreHandleImpl::OnWebsiteDataStoreUpdated,
            base::Unretained(this)));
  }

  ~SystemCookieStoreHandleImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  CRWWKHTTPCookieStore* cookie_store() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cookie_store_;
  }

  void OnWebsiteDataStoreUpdated(WKWebsiteDataStore* website_data_store) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    cookie_store_.websiteDataStore = website_data_store;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  __strong CRWWKHTTPCookieStore* cookie_store_ = nil;
  base::CallbackListSubscription subscription_;
};

std::pair<std::unique_ptr<net::SystemCookieStore>,
          std::unique_ptr<SystemCookieStoreHandle>>
CreateSystemCookieStore(BrowserState* browser_state) {
  // Using WKHTTPCookieStore guarantee that cookies are always in sync and
  // allows SystemCookieStore to handle cookies for OffTheRecord browser.
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);

  auto handle = std::make_unique<SystemCookieStoreHandleImpl>(config_provider);
  auto cookie_store = handle->cookie_store();

  return std::make_pair(
      std::make_unique<web::WKHTTPSystemCookieStore>(cookie_store),
      std::move(handle));
}

}  // namespace web
