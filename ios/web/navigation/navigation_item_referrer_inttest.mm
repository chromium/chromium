// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <memory>
#import <string>
#import <vector>

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/synchronization/lock.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/web_int_test.h"
#import "net/http/http_request_headers.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace web {

namespace {

constexpr char kFirstPagePath[] = "/first_page.html";
constexpr char kSecondPagePath[] = "/second_page.html";
constexpr char kDestinationPagePath[] = "/defaultresponse/destination";
constexpr char kOtherDestinationPagePath[] = "/defaultresponse/other";

constexpr char kSecondPageLinkId[] = "second-page-link";
constexpr char kNoReferrerLinkId[] = "no-referrer-link";
constexpr char kWithReferrerLinkId[] = "with-referrer-link";

// Thread-safe recorder for HTTP requests received by the test server.
class RequestRecorder {
 public:
  RequestRecorder() = default;
  ~RequestRecorder() = default;

  void Record(const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    auto it = request.headers.find(net::HttpRequestHeaders::kReferer);
    std::string referer = (it != request.headers.end()) ? it->second : "";
    requests_by_path_[request.relative_url].push_back(referer);
  }

  size_t GetRequestCount(const std::string& path) {
    base::AutoLock auto_lock(lock_);
    auto it = requests_by_path_.find(path);
    return it == requests_by_path_.end() ? 0 : it->second.size();
  }

  std::string GetLastReferrerForPath(const std::string& path) {
    base::AutoLock auto_lock(lock_);
    auto it = requests_by_path_.find(path);
    if (it == requests_by_path_.end() || it->second.empty()) {
      return "";
    }
    return it->second.back();
  }

 private:
  base::Lock lock_;
  std::map<std::string, std::vector<std::string>> requests_by_path_;
};

}  // namespace

// Tests the referrer //ios/web records on committed `NavigationItem`s.
class NavigationItemReferrerTest : public WebIntTest {
 protected:
  void SetUp() override {
    WebIntTest::SetUp();
    test_server_ = std::make_unique<net::EmbeddedTestServer>();
    net::test_server::RegisterDefaultHandlers(test_server_.get());
    test_server_->RegisterRequestMonitor(base::BindRepeating(
        &RequestRecorder::Record, base::Unretained(&recorder_)));

    const std::string first_page_html = base::StringPrintf(
        "<html><body><a id=\"%s\" href=\"%s\">Second Page</a></body></html>",
        kSecondPageLinkId, kSecondPagePath);
    const std::string second_page_html =
        base::StringPrintf("<html><body>"
                           "<a id=\"%s\" rel=\"noreferrer\" href=\"%s\">"
                           "Destination (No Referrer)</a>"
                           "<a id=\"%s\" href=\"%s\">Other Destination</a>"
                           "</body></html>",
                           kNoReferrerLinkId, kDestinationPagePath,
                           kWithReferrerLinkId, kOtherDestinationPagePath);
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kFirstPagePath,
        base::BindRepeating(&::testing::HandlePageWithHtml, first_page_html)));
    test_server_->RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, kSecondPagePath,
        base::BindRepeating(&::testing::HandlePageWithHtml, second_page_html)));
    ASSERT_TRUE(test_server_->Start());
  }

  // Navigates to `kFirstPagePath` and clicks the link to `kSecondPagePath` so
  // `kSecondPagePath` commits with `kFirstPagePath` as its `Referer`.
  void NavigateToSecondPage() {
    const GURL first_url = test_server_->GetURL(kFirstPagePath);
    const GURL second_url = test_server_->GetURL(kSecondPagePath);

    ASSERT_TRUE(LoadUrl(first_url));
    bool loaded = ExecuteBlockAndWaitForLoad(second_url, ^{
      EXPECT_TRUE(
          test::TapWebViewElementWithId(web_state(), kSecondPageLinkId));
    });
    ASSERT_TRUE(loaded);
  }

  // Navigates through `kFirstPagePath` -> `kSecondPagePath` link ->
  // `rel="noreferrer"` `kDestinationPagePath` link to establish the no-referrer
  // state on `kDestinationPagePath`.
  void NavigateToDestinationViaNoReferrer() {
    NavigateToSecondPage();

    const GURL destination_url = test_server_->GetURL(kDestinationPagePath);
    bool loaded = ExecuteBlockAndWaitForLoad(destination_url, ^{
      EXPECT_TRUE(
          test::TapWebViewElementWithId(web_state(), kNoReferrerLinkId));
    });
    ASSERT_TRUE(loaded);
  }

  RequestRecorder recorder_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

// Tests that the referrer recorded on the committed `NavigationItem` after
// following a `rel="noreferrer"` link is empty.
TEST_F(NavigationItemReferrerTest, RendererInitiatedNavigationWithNoReferrer) {
  const GURL first_url = test_server_->GetURL(kFirstPagePath);
  const GURL second_url = test_server_->GetURL(kSecondPagePath);
  const GURL destination_url = test_server_->GetURL(kDestinationPagePath);
  const GURL other_destination_url =
      test_server_->GetURL(kOtherDestinationPagePath);

  NavigateToSecondPage();

  NavigationItem* second_item = GetLastCommittedItem();
  ASSERT_TRUE(second_item);
  EXPECT_EQ(second_url, second_item->GetURL());
  EXPECT_EQ(first_url, second_item->GetReferrer().url);
  EXPECT_EQ(first_url.spec(),
            recorder_.GetLastReferrerForPath(kSecondPagePath));

  bool destination_loaded = ExecuteBlockAndWaitForLoad(destination_url, ^{
    EXPECT_TRUE(test::TapWebViewElementWithId(web_state(), kNoReferrerLinkId));
  });
  ASSERT_TRUE(destination_loaded);

  NavigationItem* destination_item = GetLastCommittedItem();
  ASSERT_TRUE(destination_item);
  EXPECT_EQ(destination_url, destination_item->GetURL());
  EXPECT_EQ(GURL(), destination_item->GetReferrer().url);
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kDestinationPagePath));

  // Control case: following a standard link from `kSecondPagePath` records
  // `kSecondPagePath` as the referrer on `kOtherDestinationPagePath`.
  NavigateToSecondPage();

  NavigationItem* control_second_item = GetLastCommittedItem();
  ASSERT_TRUE(control_second_item);
  EXPECT_EQ(second_url, control_second_item->GetURL());
  EXPECT_EQ(first_url, control_second_item->GetReferrer().url);

  bool other_destination_loaded =
      ExecuteBlockAndWaitForLoad(other_destination_url, ^{
        EXPECT_TRUE(
            test::TapWebViewElementWithId(web_state(), kWithReferrerLinkId));
      });
  ASSERT_TRUE(other_destination_loaded);

  NavigationItem* other_destination_item = GetLastCommittedItem();
  ASSERT_TRUE(other_destination_item);
  EXPECT_EQ(other_destination_url, other_destination_item->GetURL());
  EXPECT_EQ(second_url, other_destination_item->GetReferrer().url);
  EXPECT_EQ(second_url.spec(),
            recorder_.GetLastReferrerForPath(kOtherDestinationPagePath));
}

// Tests that the referrer recorded on committed `NavigationItem`s for
// browser-initiated (typed) loads is empty.
TEST_F(NavigationItemReferrerTest, BrowserInitiatedLoadWithNoReferrer) {
  const GURL first_url = test_server_->GetURL(kFirstPagePath);
  const GURL destination_url = test_server_->GetURL(kDestinationPagePath);
  const GURL other_destination_url =
      test_server_->GetURL(kOtherDestinationPagePath);

  NavigateToSecondPage();
  NavigationItem* second_item = GetLastCommittedItem();
  ASSERT_TRUE(second_item);
  EXPECT_EQ(first_url, second_item->GetReferrer().url);
  EXPECT_EQ(first_url.spec(),
            recorder_.GetLastReferrerForPath(kSecondPagePath));

  ASSERT_TRUE(LoadUrl(destination_url));
  NavigationItem* destination_item = GetLastCommittedItem();
  ASSERT_TRUE(destination_item);
  EXPECT_EQ(destination_url, destination_item->GetURL());
  EXPECT_EQ(GURL(), destination_item->GetReferrer().url);
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kDestinationPagePath));

  ASSERT_TRUE(LoadUrl(other_destination_url));
  NavigationItem* other_destination_item = GetLastCommittedItem();
  ASSERT_TRUE(other_destination_item);
  EXPECT_EQ(other_destination_url, other_destination_item->GetURL());
  EXPECT_EQ(GURL(), other_destination_item->GetReferrer().url);
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kOtherDestinationPagePath));
}

// Tests that `Reload(web::ReloadType::NORMAL, ...)` preserves an empty
// referrer on `kDestinationPagePath`.
TEST_F(NavigationItemReferrerTest, ReloadNormalWithNoReferrer) {
  const GURL destination_url = test_server_->GetURL(kDestinationPagePath);

  NavigateToDestinationViaNoReferrer();

  EXPECT_EQ(1u, recorder_.GetRequestCount(kDestinationPagePath));
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kDestinationPagePath));

  NavigationItem* initial_item = GetLastCommittedItem();
  ASSERT_TRUE(initial_item);
  EXPECT_EQ(destination_url, initial_item->GetURL());
  EXPECT_EQ(GURL(), initial_item->GetReferrer().url);

  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(destination_url, ^{
    navigation_manager()->Reload(web::ReloadType::NORMAL,
                                 /*check_for_repost=*/true);
  }));

  EXPECT_EQ(2u, recorder_.GetRequestCount(kDestinationPagePath));
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kDestinationPagePath));

  NavigationItem* reloaded_item = GetLastCommittedItem();
  ASSERT_TRUE(reloaded_item);
  EXPECT_EQ(destination_url, reloaded_item->GetURL());
  EXPECT_EQ(GURL(), reloaded_item->GetReferrer().url);
}

// Tests that `GoBack()` and `GoForward()` across typed `kFirstPagePath` ->
// `kSecondPagePath` link -> `rel="noreferrer"` `kDestinationPagePath` link
// preserve the expected referrers on each `NavigationItem`.
TEST_F(NavigationItemReferrerTest, BackForwardNavigationWithNoReferrer) {
  const GURL first_url = test_server_->GetURL(kFirstPagePath);
  const GURL second_url = test_server_->GetURL(kSecondPagePath);
  const GURL destination_url = test_server_->GetURL(kDestinationPagePath);

  NavigateToDestinationViaNoReferrer();

  NavigationItem* destination_item = GetLastCommittedItem();
  ASSERT_TRUE(destination_item);
  EXPECT_EQ(destination_url, destination_item->GetURL());
  EXPECT_EQ(GURL(), destination_item->GetReferrer().url);

  EXPECT_EQ(1u, recorder_.GetRequestCount(kFirstPagePath));
  EXPECT_EQ(1u, recorder_.GetRequestCount(kSecondPagePath));
  EXPECT_EQ(1u, recorder_.GetRequestCount(kDestinationPagePath));
  EXPECT_EQ(first_url.spec(),
            recorder_.GetLastReferrerForPath(kSecondPagePath));
  EXPECT_EQ("", recorder_.GetLastReferrerForPath(kDestinationPagePath));

  size_t second_requests_before = recorder_.GetRequestCount(kSecondPagePath);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(second_url, ^{
    navigation_manager()->GoBack();
  }));
  NavigationItem* second_item = GetLastCommittedItem();
  ASSERT_TRUE(second_item);
  EXPECT_EQ(second_url, second_item->GetURL());
  EXPECT_EQ(first_url, second_item->GetReferrer().url);
  // Back/forward navigation is served from `WKWebView`'s cache without a
  // network request.
  EXPECT_EQ(second_requests_before, recorder_.GetRequestCount(kSecondPagePath));

  size_t first_requests_before = recorder_.GetRequestCount(kFirstPagePath);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(first_url, ^{
    navigation_manager()->GoBack();
  }));
  NavigationItem* first_item = GetLastCommittedItem();
  ASSERT_TRUE(first_item);
  EXPECT_EQ(first_url, first_item->GetURL());
  EXPECT_EQ(GURL(), first_item->GetReferrer().url);
  EXPECT_EQ(first_requests_before, recorder_.GetRequestCount(kFirstPagePath));

  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(second_url, ^{
    navigation_manager()->GoForward();
  }));
  NavigationItem* forward_second_item = GetLastCommittedItem();
  ASSERT_TRUE(forward_second_item);
  EXPECT_EQ(second_url, forward_second_item->GetURL());
  EXPECT_EQ(first_url, forward_second_item->GetReferrer().url);
  EXPECT_EQ(second_requests_before, recorder_.GetRequestCount(kSecondPagePath));

  size_t destination_requests_before =
      recorder_.GetRequestCount(kDestinationPagePath);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(destination_url, ^{
    navigation_manager()->GoForward();
  }));
  NavigationItem* forward_destination_item = GetLastCommittedItem();
  ASSERT_TRUE(forward_destination_item);
  EXPECT_EQ(destination_url, forward_destination_item->GetURL());
  EXPECT_EQ(GURL(), forward_destination_item->GetReferrer().url);
  // Served from `WKWebView`'s cache without a network request.
  EXPECT_EQ(destination_requests_before,
            recorder_.GetRequestCount(kDestinationPagePath));
}

}  // namespace web
