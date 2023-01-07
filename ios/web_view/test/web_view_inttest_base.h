// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_INTTEST_BASE_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_INTTEST_BASE_H_

#import <Foundation/Foundation.h>
#include <memory>
#include <string>

#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/platform_test.h"

NS_ASSUME_NONNULL_BEGIN

namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

@class CWVWebView;
class GURL;
@class NSURL;

namespace ios_web_view {

// A test fixture for testing CWVWebView. A test server is also created to
// support loading content. The server supports the urls returned by the GetUrl*
// methods below.
class WebViewInttestBase : public PlatformTest {
 protected:
  WebViewInttestBase();
  ~WebViewInttestBase() override;

  // Returns URL to an html page with title set to |title|.
  //
  // Call ASSERT_TRUE(test_server_->Start()) before accessing the returned URL.
  GURL GetUrlForPageWithTitle(const std::string& title);

  // Returns URL to an html page with |html| within page's body tags.
  //
  // Call ASSERT_TRUE(test_server_->Start()) before accessing the returned URL.
  GURL GetUrlForPageWithHtmlBody(const std::string& html);

  // Returns URL to an html page with title set to |title| and |body| within
  // the page's body tags.
  //
  // Call ASSERT_TRUE(test_server_->Start()) before accessing the returned URL.
  GURL GetUrlForPageWithTitleAndBody(const std::string& title,
                                     const std::string& body);

  // Returns URL to an html page with |html|. |html| contains entire html of the
  // page.
  //
  // Call ASSERT_TRUE(test_server_->Start()) before accessing the returned URL.
  GURL GetUrlForPageWithHtml(const std::string& html);

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  // CWVWebView created with default configuration and frame equal to screen
  // bounds.
  CWVWebView* web_view_;

  // Embedded server for handling requests sent to the URLs returned by the
  // GetURL* methods.
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
};

}  // namespace ios_web_view

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_INTTEST_BASE_H_
