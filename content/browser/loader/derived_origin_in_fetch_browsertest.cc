// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using DerivedOriginInFetchBrowserTest = ContentBrowserTest;

// A fetch originating in a context with a file:/// origin should have its
// initiator derived from the originating context's origin.
IN_PROC_BROWSER_TEST_F(DerivedOriginInFetchBrowserTest,
                       FetchFromFileOriginProducesDerivedOrigin) {
  // The request will be intercepted, so this page doesn't need to exist:
  GURL destination("https://destination.irrelevant");
  // This needs to be created before navigating to the source page.
  URLLoaderMonitor monitor({destination});

  GURL starting_file_url = GetTestUrl(/*dir=*/nullptr, "title1.html");
  ASSERT_TRUE(starting_file_url.SchemeIsFile());
  ASSERT_TRUE(NavigateToURL(shell(), starting_file_url));

  ExecuteScriptAsync(shell(), JsReplace("fetch($1);", destination));
  monitor.WaitForUrls();
  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(destination);

  ASSERT_TRUE(request);
  const std::optional<url::Origin>& initiator = request->request_initiator;
  ASSERT_TRUE(initiator);
  EXPECT_TRUE(initiator->CanBeDerivedFrom(starting_file_url));
}

}  // namespace content
