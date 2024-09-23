// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/mem/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include <string>
#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "components/policy/content/safe_search_service.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_main_parts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage1Title[] = "title 1";
constexpr char kCustomErrorPageTitle[] = "Custom Error Page Title";
constexpr char kCustomExplicitSitesErrorPage[] = R"(<html>
<head><title>Custom Error Page Title</title></head>
<body>
<b>This is a custom error</b>
</body>
</html>)";

// Creates a Fuchsia memory data from |data|.
// |data| should be a short string to avoid exceeding Zircon channel limits.
fuchsia::mem::Data MemDataBytesFromShortString(std::string_view data) {
  return fuchsia::mem::Data::WithBytes(
      std::vector<uint8_t>(data.begin(), data.end()));
}

}  // namespace

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class ExplicitSitesFilterTest : public FrameImplTestBaseWithServer {
 public:
  ExplicitSitesFilterTest() = default;
  ~ExplicitSitesFilterTest() override = default;
  ExplicitSitesFilterTest(const ExplicitSitesFilterTest&) = delete;
  ExplicitSitesFilterTest& operator=(const ExplicitSitesFilterTest&) = delete;

  void SetUpOnMainThread() override {
    FrameImplTestBaseWithServer::SetUpOnMainThread();

    // Spin the message loop to allow the Context to connect, before
    // |context_impl()| is called.
    base::RunLoop().RunUntilIdle();

    SetSafeSearchURLCheckerForBrowserContext(context_impl()->browser_context());
  }

 protected:
  const size_t kUrlCheckerCacheSize = 1;

  void SetPageIsNotExplicit() {
    stub_url_checker_.SetUpValidResponse(false /* is_porn */);
  }

  void SetPageIsExplicit() {
    stub_url_checker_.SetUpValidResponse(true /* is_porn */);
  }

  std::string GetPage1UrlSpec() const {
    return embedded_test_server()->GetURL(kPage1Path).spec();
  }

  fuchsia::web::FrameHostPtr ConnectToFrameHost() {
    fuchsia::web::FrameHostPtr frame_host;
    zx_status_t status = published_services().Connect(frame_host.NewRequest());
    ZX_CHECK(status == ZX_OK, status) << "Connect to fuchsia.web.FrameHost";
    base::RunLoop().RunUntilIdle();
    return frame_host;
  }

  void SetSafeSearchURLCheckerForBrowserContext(
      content::BrowserContext* browser_context) {
    SafeSearchFactory::GetInstance()
        ->GetForBrowserContext(browser_context)
        ->SetSafeSearchURLCheckerForTest(
            stub_url_checker_.BuildURLChecker(kUrlCheckerCacheSize));
  }

  safe_search_api::StubURLChecker stub_url_checker_;
};

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FilterDisabled_SiteAllowed) {
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  SetPageIsNotExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FilterDisabled_SiteBlocked) {
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  SetPageIsExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, DefaultErrorPage_SiteAllowed) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      fuchsia::mem::Data::WithBytes({}));
  auto frame = FrameForTest::Create(context(), std::move(params));

  SetPageIsNotExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, DefaultErrorPage_SiteBlocked) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      fuchsia::mem::Data::WithBytes({}));
  auto frame = FrameForTest::Create(context(), std::move(params));

  SetPageIsExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  // The page title is the URL for which navigation failed without the scheme
  // part ("http://");
  std::string expected_title = GetPage1UrlSpec().erase(0, 7);
  frame.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
      expected_title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, CustomErrorPage_SiteAllowed) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame = FrameForTest::Create(context(), std::move(params));

  SetPageIsNotExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, CustomErrorPage_SiteBlocked) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame = FrameForTest::Create(context(), std::move(params));

  SetPageIsExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
      kCustomErrorPageTitle);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FrameHost_SiteAllowed) {
  fuchsia::web::FrameHostPtr frame_host = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame = FrameForTest::Create(frame_host, std::move(params));

  SetPageIsNotExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FrameHost_SiteBlocked) {
  fuchsia::web::FrameHostPtr frame_host = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame = FrameForTest::Create(frame_host, std::move(params));

  SetPageIsExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
      kCustomErrorPageTitle);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest,
                       MultipleFrameHosts_SiteAllowed) {
  fuchsia::web::FrameHostPtr frame_host1 = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame1 = FrameForTest::Create(frame_host1, std::move(params));

  SetPageIsNotExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame1.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame1.navigation_listener().RunUntilTitleEquals(kPage1Title);

  // Disconnect first FrameHost, causing the associated BrowserContext to be
  // deleted. Then, create a new FrameHost connection, which creates a new
  // BrowserContext.
  frame_host1.Unbind();
  frame1 = {};

  fuchsia::web::FrameHostPtr frame_host2 = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params2;
  params2.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame2 = FrameForTest::Create(frame_host2, std::move(params2));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame2.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame2.navigation_listener().RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest,
                       MultipleFrameHosts_SiteBlocked) {
  fuchsia::web::FrameHostPtr frame_host1 = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame1 = FrameForTest::Create(frame_host1, std::move(params));

  SetPageIsExplicit();

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame1.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame1.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
      kCustomErrorPageTitle);

  // Disconnect first FrameHost, causing the associated BrowserContext to be
  // deleted. Then, create a new FrameHost connection, which creates a new
  // BrowserContext.
  frame_host1.Unbind();
  frame1 = {};

  fuchsia::web::FrameHostPtr frame_host2 = ConnectToFrameHost();
  ASSERT_EQ(frame_host_impls().size(), 1U);

  SetSafeSearchURLCheckerForBrowserContext(
      frame_host_impls().front()->context_impl_for_test()->browser_context());

  fuchsia::web::CreateFrameParams params2;
  params2.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));
  auto frame2 = FrameForTest::Create(frame_host2, std::move(params2));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame2.GetNavigationController(), {},
                                       GetPage1UrlSpec()));

  frame2.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
      kCustomErrorPageTitle);
}
