// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/task/sequenced_task_runner.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/common/test/url_request_rewrite_test_util.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "fuchsia_web/webengine/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage2Path[] = "/title2.html";
constexpr char kPage3Path[] = "/image.html";
constexpr char kPage1Title[] = "title 1";
constexpr char kPage3ImgPath[] = "/img.png";
constexpr char kSetHeaderRequestPath[] = "/set_header_request.html";
constexpr char kWebWorkerPostLoadedPath[] = "/post_loaded.js";
constexpr char kWebWorkerImportScriptPath[] = "/import_script.js";
constexpr char kNestedWebWorkerPath[] = "/nested_worker.js";
constexpr char kSharedWorkerPostLoadedPath[] = "/shared_post_loaded.js";
constexpr char kPageWithWebWorkerPath[] = "/web_worker.html?worker_url=";
constexpr char kPageWithSharedWorkerPath[] = "/shared_worker.html?worker_url=";
constexpr char kWorkerLoadedMessage[] = "loaded";

class RequestMonitoringTest : public FrameImplTestBase {
 public:
  RequestMonitoringTest() = default;
  ~RequestMonitoringTest() override = default;

  RequestMonitoringTest(const RequestMonitoringTest&) = delete;
  RequestMonitoringTest& operator=(const RequestMonitoringTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    // Accumulate all http requests made to |embedded_test_server| into
    // |accumulated_requests_| container.
    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&RequestMonitoringTest::MonitorRequestOnIoThread,
                            base::Unretained(this),
                            base::SequencedTaskRunner::GetCurrentDefault()));

    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Needed for UrlRequestRewriteAddHeaders.
    command_line->AppendSwitchNative(switches::kCorsExemptHeaders, "Test");
  }

  std::map<GURL, net::test_server::HttpRequest> accumulated_requests_;

 private:
  void MonitorRequestOnIoThread(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const net::test_server::HttpRequest& request) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&RequestMonitoringTest::MonitorRequestOnMainThread,
                       base::Unretained(this), request));
  }

  void MonitorRequestOnMainThread(
      const net::test_server::HttpRequest& request) {
    accumulated_requests_[request.GetURL()] = request;
  }

  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, ExtraHeaders) {
  auto frame = FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  fuchsia::web::LoadUrlParams load_url_params;
  fuchsia::net::http::Header header1;
  header1.name = StringToBytes("X-ExtraHeaders");
  header1.value = StringToBytes("1");
  fuchsia::net::http::Header header2;
  header2.name = StringToBytes("X-2ExtraHeaders");
  header2.value = StringToBytes("2");
  load_url_params.set_headers({header1, header2});

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       std::move(load_url_params),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page_url, kPage1Title);

  // At this point, the page should be loaded, the server should have received
  // the request and the request should be in the map.
  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-ExtraHeaders")));
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-2ExtraHeaders")));
}

// Tests that UrlRequestActions can be set up to deny requests to specific
// hosts.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteDeny) {
  auto frame = FrameForTest::Create(context(), {});

  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_hosts_filter({"127.0.0.1"});
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be blocked.
  const GURL page_url(embedded_test_server()->GetURL(kPage3Path));
  {
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }

  // However, "localhost" is not blocked, so this request should be allowed.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url_localhost.spec()));
    frame.navigation_listener().RunUntilUrlEquals(page_url_localhost);
  }
}

// Tests that a UrlRequestAction with no filter criteria will apply to all
// requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteDenyAll) {
  auto frame = FrameForTest::Create(context(), {});

  // No filter criteria are set, so everything is denied.
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be blocked.
  const GURL page_url(embedded_test_server()->GetURL(kPage3Path));
  {
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }

  // "localhost" should be blocked.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url_localhost.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }
}

// Tests that UrlRequestActions can be set up to only allow requests for a
// single host, while denying everything else.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteSelectiveAllow) {
  auto frame = FrameForTest::Create(context(), {});

  // Allow 127.0.0.1.
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_hosts_filter({"127.0.0.1"});
  rule.set_action(fuchsia::web::UrlRequestAction::ALLOW);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));

  // Deny everything else.
  rule = {};
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be allowed.
  const GURL page_url(embedded_test_server()->GetURL(kPage3Path));
  {
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(page_url);
  }

  // "localhost" should be blocked.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url_localhost.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }
}

// Tests the URLRequestRewrite API properly adds headers on every requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteAddHeaders) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(embedded_test_server()->GetURL(kPage3Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage3ImgPath));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
}

// Tests that URLRequestRewrite applies to worker scripts loaded by the page.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteAddHeadersForWebWorkers) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(
      embedded_test_server()->GetURL(std::string(kPageWithWebWorkerPath) +
                                     std::string(kWebWorkerPostLoadedPath)));
  const GURL worker_url(
      embedded_test_server()->GetURL(kWebWorkerPostLoadedPath));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals(kWorkerLoadedMessage);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(worker_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
}

// Tests that URLRequestRewrite applies to scripts imported by workers on the
// page.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteAddHeadersForWebWorkersImportScripts) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(
      embedded_test_server()->GetURL(std::string(kPageWithWebWorkerPath) +
                                     std::string(kWebWorkerImportScriptPath)));
  const GURL worker_url(
      embedded_test_server()->GetURL(kWebWorkerImportScriptPath));
  const GURL imported_url(
      embedded_test_server()->GetURL(kWebWorkerPostLoadedPath));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals(kWorkerLoadedMessage);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(worker_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(imported_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
}

// Tests that URLRequestRewrite applies to nested workers on the page.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteAddHeadersForNestedWebWorkers) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(embedded_test_server()->GetURL(
      std::string(kPageWithWebWorkerPath) + std::string(kNestedWebWorkerPath)));
  const GURL worker_url(embedded_test_server()->GetURL(kNestedWebWorkerPath));
  const GURL nested_url(
      embedded_test_server()->GetURL(kWebWorkerPostLoadedPath));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals(kWorkerLoadedMessage);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(worker_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(nested_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
}

// Tests that URLRequestRewrite does not apply to shared workers on the page.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteDoesNotAddHeadersForSharedWorkers) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(
      embedded_test_server()->GetURL(std::string(kPageWithSharedWorkerPath) +
                                     std::string(kSharedWorkerPostLoadedPath)));
  const GURL worker_url(
      embedded_test_server()->GetURL(kSharedWorkerPostLoadedPath));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals(kWorkerLoadedMessage);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(worker_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers,
                testing::Not(testing::Contains(testing::Key("Test"))));
  }
}

// Tests the URLRequestRewrite API properly adds headers on every requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteAddExistingHeader) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate. The first page request should have the "Test" header set to
  // "Value". The second one should have the "Test" header set to "SetByJS" via
  // JavaScript and not be overridden by the rewrite rule.
  const GURL page_url(embedded_test_server()->GetURL(kSetHeaderRequestPath));
  const GURL img_url(embedded_test_server()->GetURL(kPage3Path));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals("loaded");

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    ASSERT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
    EXPECT_EQ(iter->second.headers["Test"], "Value");
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    ASSERT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
    EXPECT_EQ(iter->second.headers["Test"], "SetByJS");
  }
}

// Tests the URLRequestRewrite API properly removes headers on every requests.
// Also tests that rewrites are applied properly in succession.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteRemoveHeader) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(CreateRewriteRemoveHeader(std::nullopt, "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get no "Test" header.
  const GURL page_url(embedded_test_server()->GetURL(kPage3Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage3ImgPath));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers,
                testing::Not(testing::Contains(testing::Key("Test"))));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers,
                testing::Not(testing::Contains(testing::Key("Test"))));
  }
}

// Tests the URLRequestRewrite API properly removes headers, based on the
// presence of a string in the query.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteRemoveHeaderWithQuery) {
  auto frame = FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL("/page?stuff=[pattern]"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(
      CreateRewriteRemoveHeader(std::make_optional("[pattern]"), "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get no "Test" header.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test"))));
}

// Tests the URLRequestRewrite API properly handles query substitution.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteSubstituteQueryPattern) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(
      CreateRewriteSubstituteQueryPattern("[pattern]", "substitution"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the URL with the modified request.
  const GURL page_url(embedded_test_server()->GetURL("/page?[pattern]"));
  const GURL final_url(embedded_test_server()->GetURL("/page?substitution"));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteReplaceUrl) {
  auto frame = FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  const GURL final_url(embedded_test_server()->GetURL(kPage2Path));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteReplaceUrl(kPage1Path, final_url.spec()));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement when the
// original request URL contains a query and a fragment string.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteReplaceUrlQueryRef) {
  auto frame = FrameForTest::Create(context(), {});

  const GURL page_url(
      embedded_test_server()->GetURL(std::string(kPage1Path) + "?query#ref"));
  const GURL replacement_url(embedded_test_server()->GetURL(kPage2Path));
  const GURL final_url_with_ref(
      embedded_test_server()->GetURL(std::string(kPage2Path) + "?query#ref"));
  const GURL final_url(
      embedded_test_server()->GetURL(std::string(kPage2Path) + "?query"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(
      CreateRewriteReplaceUrl(kPage1Path, replacement_url.spec()));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url_with_ref);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles adding a query.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest, UrlRequestRewriteAddQuery) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAppendToQuery("query"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  {
    // Add a query to a URL with no query.
    const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
    const GURL expected_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "?query"));

    // Navigate, we should get to the URL with the query.
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_,
                testing::Contains(testing::Key(expected_url)));
  }

  {
    // Add a quest to a URL that has an empty query.
    const std::string original_path = std::string(kPage1Path) + "?";
    const GURL page_url(embedded_test_server()->GetURL(original_path));
    const GURL expected_url(
        embedded_test_server()->GetURL(original_path + "query"));

    // Navigate, we should get to the URL with the query, but no "&".
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_,
                testing::Contains(testing::Key(expected_url)));
  }

  {
    // Add a query to a URL that already has a query.
    const std::string original_path =
        std::string(kPage1Path) + "?original_query=value";
    const GURL page_url(embedded_test_server()->GetURL(original_path));
    const GURL expected_url(
        embedded_test_server()->GetURL(original_path + "&query"));

    // Navigate, we should get to the URL with the appended query.
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_,
                testing::Contains(testing::Key(expected_url)));
  }

  {
    // Add a query to a URL that has a ref.
    const GURL page_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "#ref"));
    const GURL expected_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "?query#ref"));

    // Navigate, we should get to the URL with the query.
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);
  }
}

// Tests the URLRequestRewrite API properly handles adding a query with a
// question mark.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteAppendToQueryQuestionMark) {
  auto frame = FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  const GURL expected_url(
      embedded_test_server()->GetURL(std::string(kPage1Path) + "?qu?ery"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(CreateRewriteAppendToQuery("qu?ery"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the URL with the query.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(expected_url);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(expected_url)));
}

// Tests the URLRequestRewrite API properly handles scheme and host filtering in
// rules.
IN_PROC_BROWSER_TEST_F(RequestMonitoringTest,
                       UrlRequestRewriteSchemeHostFilter) {
  auto frame = FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites1;
  rewrites1.push_back(CreateRewriteAddHeaders("Test1", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule1;
  rule1.set_rewrites(std::move(rewrites1));
  rule1.set_hosts_filter({"127.0.0.1"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites2;
  rewrites2.push_back(CreateRewriteAddHeaders("Test2", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule2;
  rule2.set_rewrites(std::move(rewrites2));
  rule2.set_hosts_filter({"test.xyz"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites3;
  rewrites3.push_back(CreateRewriteAddHeaders("Test3", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule3;
  rule3.set_rewrites(std::move(rewrites3));
  rule3.set_schemes_filter({"http"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites4;
  rewrites4.push_back(CreateRewriteAddHeaders("Test4", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule4;
  rule4.set_rewrites(std::move(rewrites4));
  rule4.set_schemes_filter({"https"});

  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule1));
  rules.push_back(std::move(rule2));
  rules.push_back(std::move(rule3));
  rules.push_back(std::move(rule4));

  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the "Test1" and "Test3" headers, but not "Test2"
  // and "Test4".
  const GURL page_url(embedded_test_server()->GetURL("/default"));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test1")));
  EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test3")));
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test2"))));
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test4"))));
}

}  // namespace
