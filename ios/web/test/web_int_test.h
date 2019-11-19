// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_INT_TEST_H_
#define IOS_WEB_TEST_WEB_INT_TEST_H_

#import <WebKit/WebKit.h>

#include "base/compiler_specific.h"
#import "base/ios/block_types.h"
#include "base/macros.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_state.h"

class GURL;

namespace web {

// A test fixture for integration tests that need to bring up the HttpServer.
class WebIntTest : public WebTest {
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

  // Returns the last committed NavigationItem in |navigation_manager|.
  NavigationItem* GetLastCommittedItem() {
    return navigation_manager()->GetLastCommittedItem();
  }

  // Synchronously executes |script| on |web_state|'s JS injection receiver and
  // returns the result.
  id ExecuteJavaScript(NSString* script);

  // Executes |block| and waits until |url| is successfully loaded in
  // |web_state_|.
  bool ExecuteBlockAndWaitForLoad(const GURL& url,
                                  ProceduralBlock block) WARN_UNUSED_RESULT;

  // Navigates |web_state_| to |url| and waits for the page to be loaded.
  bool LoadUrl(const GURL& url) WARN_UNUSED_RESULT;

  // Navigates |web_state_| using |params| and waits for the page to be loaded.
  bool LoadWithParams(const NavigationManager::WebLoadParams& params)
      WARN_UNUSED_RESULT;

  // Synchronously removes data from |data_store|.
  // |websiteDataTypes| is from the constants defined in
  // "WebKit/WKWebsiteDataRecord".
  void RemoveWKWebViewCreatedData(WKWebsiteDataStore* data_store,
                                  NSSet* websiteDataTypes);

  // Returns the index of |item| in the |navigation_manager|'s session history,
  // or NSNotFound if it is not present.
  NSInteger GetIndexOfNavigationItem(const web::NavigationItem* item);

  web::TestWebStateDelegate web_state_delegate_;

 private:
  // WebState used to load pages.
  std::unique_ptr<WebState> web_state_;

  DISALLOW_COPY_AND_ASSIGN(WebIntTest);
};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_INT_TEST_H_
