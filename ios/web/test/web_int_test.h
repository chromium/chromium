// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_INT_TEST_H_
#define IOS_WEB_TEST_WEB_INT_TEST_H_

#import <WebKit/WebKit.h>

#import "base/ios/block_types.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_state.h"

class GURL;

namespace web {

// A test fixture for integration tests that need a WebState which loads pages.
class WebIntTest : public WebTest {
 public:
  WebIntTest(const WebIntTest&) = delete;
  WebIntTest& operator=(const WebIntTest&) = delete;

 protected:
  WebIntTest();
  ~WebIntTest() override;

  // WebTest methods.
  void SetUp() override;
  void TearDown() override;

  // The WebState and NavigationManager used by this test fixture.
  WebState* web_state() { return web_state_.get(); }
  NavigationManager* navigation_manager() {
    return web_state()->GetNavigationManager();
  }

  // Returns the last committed NavigationItem in `navigation_manager`.
  NavigationItem* GetLastCommittedItem() {
    return navigation_manager()->GetLastCommittedItem();
  }

  // Executes `block` and waits until `url` is successfully loaded in
  // `web_state_`.
  [[nodiscard]] bool ExecuteBlockAndWaitForLoad(const GURL& url,
                                                ProceduralBlock block);

  // Navigates `web_state_` to `url` and waits for the page to be loaded.
  [[nodiscard]] bool LoadUrl(const GURL& url);

  // Navigates `web_state_` using `params` and waits for the page to be loaded.
  [[nodiscard]] bool LoadWithParams(
      const NavigationManager::WebLoadParams& params);

  // Synchronously removes data from `data_store`.
  // `websiteDataTypes` is from the constants defined in
  // "WebKit/WKWebsiteDataRecord".
  void RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                  NSSet* websiteDataTypes);

  // Returns the index of `item` in the `navigation_manager`'s session history,
  // or NSNotFound if it is not present.
  NSInteger GetIndexOfNavigationItem(const web::NavigationItem* item);

  web::FakeWebStateDelegate web_state_delegate_;

 private:
  // WebState used to load pages.
  std::unique_ptr<WebState> web_state_;
};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_INT_TEST_H_
