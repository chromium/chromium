// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include <string>

#include "components/policy/content/safe_search_service.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/frame_impl_browser_test_base.h"
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
fuchsia::mem::Data MemDataBytesFromShortString(base::StringPiece data) {
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

    SafeSearchFactory::GetInstance()
        ->GetForBrowserContext(context_impl()->browser_context_for_test())
        ->SetSafeSearchURLCheckerForTest(
            stub_url_checker_.BuildURLChecker(kUrlCheckerCacheSize));
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

  safe_search_api::StubURLChecker stub_url_checker_;
};

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FilterDisabled_SiteAllowed) {
  fuchsia::web::CreateFrameParams params;

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsNotExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  navigation_listener_.RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, FilterDisabled_SiteBlocked) {
  fuchsia::web::CreateFrameParams params;

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  navigation_listener_.RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, DefaultErrorPage_SiteAllowed) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      fuchsia::mem::Data::WithBytes({}));

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsNotExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  navigation_listener_.RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, DefaultErrorPage_SiteBlocked) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      fuchsia::mem::Data::WithBytes({}));

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  // The page title is the URL for which navigation failed without the scheme
  // part ("http://");
  std::string expected_title = GetPage1UrlSpec().erase(0, 7);
  navigation_listener_.RunUntilErrorPageIsLoadedAndTitleEquals(expected_title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, CustomErrorPage_SiteAllowed) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsNotExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  navigation_listener_.RunUntilTitleEquals(kPage1Title);
}

IN_PROC_BROWSER_TEST_F(ExplicitSitesFilterTest, CustomErrorPage_SiteBlocked) {
  fuchsia::web::CreateFrameParams params;
  params.set_explicit_sites_filter_error_page(
      MemDataBytesFromShortString(kCustomExplicitSitesErrorPage));

  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrameWithParams(
      &navigation_listener_, std::move(params));

  SetPageIsExplicit();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   GetPage1UrlSpec()));

  navigation_listener_.RunUntilErrorPageIsLoadedAndTitleEquals(
      kCustomErrorPageTitle);
}
