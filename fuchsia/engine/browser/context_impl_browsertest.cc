// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/result_receiver.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace {

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class ContextImplTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  ContextImplTest() = default;
  ~ContextImplTest() override = default;

  ContextImplTest(const ContextImplTest&) = delete;
  ContextImplTest& operator=(const ContextImplTest&) = delete;

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  // Synchronously gets the list of all cookies from the fuchsia.web.Context.
  std::vector<fuchsia::web::Cookie> GetCookies() {
    base::RunLoop get_cookies_loop;

    // Connect to the Context's CookieManager and request all the cookies.
    fuchsia::web::CookieManagerPtr cookie_manager;
    context()->GetCookieManager(cookie_manager.NewRequest());
    fuchsia::web::CookiesIteratorPtr cookies_iterator;
    cookie_manager->GetCookieList(nullptr, nullptr,
                                  cookies_iterator.NewRequest());

    // |cookies_iterator| will disconnect once after the last cookies have been
    // returned by GetNext().
    cookies_iterator.set_error_handler([&](zx_status_t status) {
      EXPECT_EQ(ZX_ERR_PEER_CLOSED, status);
      get_cookies_loop.Quit();
    });
    std::vector<fuchsia::web::Cookie> cookies;

    // std::function<> must be used here because fit::function<> is move-only
    // and this callback will be used both for the initial GetNext() call, and
    // for the follow-up calls made each time GetNext() results are received.
    std::function<void(std::vector<fuchsia::web::Cookie>)> get_next_callback =
        [&](std::vector<fuchsia::web::Cookie> new_cookies) {
          cookies.insert(cookies.end(),
                         std::make_move_iterator(new_cookies.begin()),
                         std::make_move_iterator(new_cookies.end()));
          cookies_iterator->GetNext(get_next_callback);
        };
    cookies_iterator->GetNext(get_next_callback);

    get_cookies_loop.Run();

    return cookies;
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;
};

fuchsia::web::Cookie CreateExpectedCookie() {
  fuchsia::web::Cookie cookie;
  fuchsia::web::CookieId id;
  id.set_name("foo");
  id.set_path("/");
  id.set_domain("127.0.0.1");
  cookie.set_id(std::move(id));
  cookie.set_value("bar");
  return cookie;
}

const fuchsia::web::Cookie& ExpectedCookie() {
  static const base::NoDestructor<fuchsia::web::Cookie> expected_cookie(
      CreateExpectedCookie());
  return *expected_cookie;
}

}  // namespace

// BrowserContext with persistent storage stores cookies such that they can
// be retrieved via the CookieManager API.
IN_PROC_BROWSER_TEST_F(ContextImplTest, PersistentCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  const GURL kSetCookieUrl(
      embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kSetCookieUrl.spec());
  navigation_listener_.RunUntilUrlEquals(kSetCookieUrl);

  std::vector<fuchsia::web::Cookie> cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  ASSERT_TRUE(cookies[0].has_id());
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));

  // Check that the cookie persists beyond the lifetime of the Frame by
  // releasing the Frame and re-querying the CookieStore.
  frame.Unbind();
  base::RunLoop().RunUntilIdle();

  cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  ASSERT_TRUE(cookies[0].has_id());
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));
}

// Suite for tests which run the BrowserContext in incognito mode (no data
// directory).
class IncognitoContextImplTest : public ContextImplTest {
 public:
  IncognitoContextImplTest() = default;
  ~IncognitoContextImplTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kIncognito);
    ContextImplTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoContextImplTest);
};

// Verify that the browser can be initialized without a persistent data
// directory.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, NavigateFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
  navigation_listener_.RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  frame.Unbind();
}

// In-memory cookie store stores cookies, and is accessible via CookieManager.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, InMemoryCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  const GURL kSetCookieUrl(
      embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kSetCookieUrl.spec());
  navigation_listener_.RunUntilUrlEquals(kSetCookieUrl);

  std::vector<fuchsia::web::Cookie> cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));
}
