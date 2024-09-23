// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

namespace content {

class ResourceLoadingBrowserTest : public ContentBrowserTest  {
};

const char kResourceLoadingNonMobilePage[] =
    "/resource_loading/resource_loading_non_mobile.html";

// TODO(crbug.com/40850567): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ResourceLoadingAvoidDoubleDownloads \
  DISABLED_ResourceLoadingAvoidDoubleDownloads
#else
#define MAYBE_ResourceLoadingAvoidDoubleDownloads \
  ResourceLoadingAvoidDoubleDownloads
#endif
IN_PROC_BROWSER_TEST_F(ResourceLoadingBrowserTest,
                       MAYBE_ResourceLoadingAvoidDoubleDownloads) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kResourceLoadingNonMobilePage);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  int resourceNumber = EvalJs(shell(), "getResourceNumber()").ExtractInt();
  // Hacky way to get the flaky extra resource timing entry content to logs.
  if (resourceNumber != 9) {
    EXPECT_EQ("", EvalJs(shell(), "getResources()"));
  }
  EXPECT_EQ(9, resourceNumber);
}

} // namespace content
