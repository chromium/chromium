// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_test_utils.h"

#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

enum class NotificationType {
  // The Cookie Access Notification arrived through the
  // OnCookiesAccessed(NavigationHandle) override.
  kNavigationHandle,
  // The Cookie Access Notification arrived through the
  // OnCookiesAccessed(RenderFrameHost) override.
  kRenderFrameHost,
};

// Class that saves the type of notification received for a cookie access.
class CookieAccessNotificationTypeObserver : public WebContentsObserver {
 public:
  explicit CookieAccessNotificationTypeObserver(WebContents& web_contents)
      : WebContentsObserver(&web_contents) {}

  CookieAccessNotificationTypeObserver(
      const CookieAccessNotificationTypeObserver&) = delete;
  CookieAccessNotificationTypeObserver& operator=(
      const CookieAccessNotificationTypeObserver&) = delete;

  ~CookieAccessNotificationTypeObserver() override = default;

  const std::vector<NotificationType>& types() const {
    return notification_types_;
  }

  // WebContentsObserver
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override {
    CHECK_NE(details.type, CookieAccessDetails::Type::kRead);
    notification_types_.push_back(NotificationType::kNavigationHandle);
  }

  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override {
    if (details.type == CookieAccessDetails::Type::kRead) {
      return;
    }

    notification_types_.push_back(NotificationType::kRenderFrameHost);
  }

 private:
  std::vector<NotificationType> notification_types_;
};

class BtmTestUtilsBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BtmTestUtilsBrowserTest,
                       NavigationalAccess_FrameNotification) {
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/set_cookie.html");

  auto& web_contents = *shell()->web_contents();

  CookieAccessNotificationTypeObserver type_observer(web_contents);
  URLCookieAccessObserver observer(&web_contents, url1,
                                   content::CookieAccessDetails::Type::kChange);

  CookieAccessInterceptor interceptor(web_contents);
  ASSERT_TRUE(NavigateToURL(&web_contents, url1));
  observer.Wait();

  ASSERT_EQ(type_observer.types().size(), 1u);
  EXPECT_EQ(type_observer.types()[0], NotificationType::kRenderFrameHost);
}

}  // namespace content
