// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

enum class CorsTestMode {
  kInBlink,
  kInBrowserProcess,
  kInNetworkService,
};

// Tests end to end Origin header and CORS check behaviors without
// --allow-file-access-from-files flag.
class CORSFileOriginBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<CorsTestMode> {
 public:
  CORSFileOriginBrowserTest()
      : pass_string_(base::ASCIIToUTF16("PASS")),
        fail_string_(base::ASCIIToUTF16("FAIL")) {
    switch (GetParam()) {
      case CorsTestMode::kInBlink:
        scoped_feature_list_.InitWithFeatures(
            {} /* enabled */,
            {network::features::kOutOfBlinkCORS,
             blink::features::kServiceWorkerServicification,
             network::features::kNetworkService} /* disabled */);
        break;
      case CorsTestMode::kInBrowserProcess:
        scoped_feature_list_.InitWithFeatures(
            {network::features::kOutOfBlinkCORS,
             blink::features::kServiceWorkerServicification} /* enabled */,
            {network::features::kNetworkService} /* disabled */);
        break;
      case CorsTestMode::kInNetworkService:
        scoped_feature_list_.InitWithFeatures(
            {network::features::kOutOfBlinkCORS,
             blink::features::kServiceWorkerServicification,
             network::features::kNetworkService} /* enabled */,
            {} /*disabled */);
        break;
    }
  }
  ~CORSFileOriginBrowserTest() override = default;

 protected:
  GURL CreateTestDataURL(const std::string& relative_path) {
    base::AutoLock lock(lock_);
    base::FilePath path(test_data_loader_path_);
    path = path.AppendASCII(relative_path);
    // This is wrong if developers checkout the source code in a path that
    // contains non-ASCII characters to run the browser tests.
    std::string url = "file://" + path.MaybeAsASCII();
    return GURL(url);
  }

  std::unique_ptr<TitleWatcher> CreateWatcher() {
    // Register all possible result strings here.
    std::unique_ptr<TitleWatcher> watcher =
        std::make_unique<TitleWatcher>(shell()->web_contents(), pass_string());
    watcher->AlsoWaitForTitle(fail_string());

    // Does not appear in the expectations, but the title can be on unexpected
    // failures.
    base::string16 wrong_origin_string =
        base::ASCIIToUTF16("FAIL: response text does not match");
    watcher->AlsoWaitForTitle(wrong_origin_string);
    return watcher;
  }

  std::string target_http_url() {
    return base::StringPrintf("http://127.0.0.1:%d/test", port());
  }
  std::string target_file_url() const { return "get.txt"; }
  std::string target_self_file_url() const {
    return "cors_file_origin_test.html";
  }

  const base::string16& pass_string() const { return pass_string_; }
  const base::string16& fail_string() const { return fail_string_; }

  uint16_t port() {
    base::AutoLock lock(lock_);
    return port_;
  }

  bool is_preflight_requested() {
    base::AutoLock lock(lock_);
    return is_preflight_requested_;
  }

 private:
  bool AllowFileAccessFromFiles() const override { return false; }
  virtual bool IsWebSecurityEnabled() const { return true; }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!IsWebSecurityEnabled()) {
      command_line->AppendSwitch(switches::kDisableWebSecurity);
    }

    ContentBrowserTest::SetUpCommandLine(command_line);
  }
  void SetUpOnMainThread() override {
    base::AutoLock lock(lock_);

    // Need to obtain the path on the main thread.
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &test_data_loader_path_));
    test_data_loader_path_ =
        test_data_loader_path_.Append(FILE_PATH_LITERAL("loader"));

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CORSFileOriginBrowserTest::HandleWithAccessControlAllowOrigin,
        base::Unretained(this)));

    ASSERT_TRUE(embedded_test_server()->Start());

    port_ = embedded_test_server()->port();
  }

  std::unique_ptr<HttpResponse> HandleWithAccessControlAllowOrigin(
      const HttpRequest& request) {
    std::unique_ptr<BasicHttpResponse> response;

    if (net::test_server::ShouldHandle(request, "/test")) {
      // Accept XHR CORS requests.
      response = std::make_unique<BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      auto query = net::test_server::ParseQuery(request.GetURL());
      response->AddCustomHeader(
          network::cors::header_names::kAccessControlAllowOrigin,
          query["allow"][0]);
      if (request.method == net::test_server::METHOD_OPTIONS) {
        // For CORS-preflight request.
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowMethods,
            "GET, OPTIONS");
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowHeaders,
            "X-NotSimple");
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlMaxAge, "0");
        response->AddCustomHeader(net::HttpRequestHeaders::kCacheControl,
                                  "no-store");
        base::AutoLock lock(lock_);
        is_preflight_requested_ = true;
      }

      // Return the request origin header as the body so that JavaScript can
      // check if it sent the expected origin header.
      auto origin = request.headers.find(net::HttpRequestHeaders::kOrigin);
      if (origin != request.headers.end())
        response->set_content(origin->second);
    }
    return response;
  }

  base::Lock lock_;
  base::FilePath test_data_loader_path_;
  uint16_t port_;
  bool is_preflight_requested_ = false;

  const base::string16 pass_string_;
  const base::string16 fail_string_;

  base::test::ScopedFeatureList scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CORSFileOriginBrowserTest);
};

// Tests end to end Origin header and CORS check behaviors with
// --allow-file-access-from-files flag.
class CORSFileOriginBrowserTestWithAllowFileAccessFromFiles
    : public CORSFileOriginBrowserTest {
 private:
  bool AllowFileAccessFromFiles() const override { return true; }
};

// Tests end to end Origin header and CORS check behaviors with
// --disable-web-security flag.
class CORSFileOriginBrowserTestWithDisableWebSecurity
    : public CORSFileOriginBrowserTest {
 private:
  bool AllowFileAccessFromFiles() const override { return false; }
  bool IsWebSecurityEnabled() const override { return false; }
};

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTest,
                       AccessControlAllowOriginIsNull) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "null", "null"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
  EXPECT_TRUE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTest,
                       AccessControlAllowOriginIsFile) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "file://", "null"))));

  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle());
  EXPECT_TRUE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTest, AccessToSelfFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_self_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTest, AccessToAnotherFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithAllowFileAccessFromFiles,
                       AccessControlAllowOriginIsNull) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "null", "file://"))));

  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle());
  EXPECT_TRUE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithAllowFileAccessFromFiles,
                       AccessControlAllowOriginIsFile) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "file://", "file://"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
  EXPECT_TRUE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithAllowFileAccessFromFiles,
                       AccessToSelfFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_self_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithAllowFileAccessFromFiles,
                       AccessToAnotherFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithDisableWebSecurity,
                       AccessControlAllowOriginIsNull) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "unused", ""))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
  EXPECT_FALSE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithDisableWebSecurity,
                       AccessControlAllowOriginIsFile) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_http_url().c_str(), "unused", ""))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
  EXPECT_FALSE(is_preflight_requested());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithDisableWebSecurity,
                       AccessToSelfFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_self_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_P(CORSFileOriginBrowserTestWithDisableWebSecurity,
                       AccessToAnotherFileUrl) {
  std::unique_ptr<TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      shell(),
      CreateTestDataURL(base::StringPrintf(
          "cors_file_origin_test.html?url=%s&allow=%s&response_text=%s",
          target_file_url().c_str(), "unused", "unused"))));

  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle());
}

INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    CORSFileOriginBrowserTest,
    ::testing::Values(CorsTestMode::kInBlink,
                      CorsTestMode::kInBrowserProcess,
                      CorsTestMode::kInNetworkService));

INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    CORSFileOriginBrowserTestWithAllowFileAccessFromFiles,
    ::testing::Values(CorsTestMode::kInBlink,
                      CorsTestMode::kInBrowserProcess,
                      CorsTestMode::kInNetworkService));

INSTANTIATE_TEST_CASE_P(
    /* No test prefix */,
    CORSFileOriginBrowserTestWithDisableWebSecurity,
    ::testing::Values(CorsTestMode::kInBlink,
                      CorsTestMode::kInBrowserProcess,
                      CorsTestMode::kInNetworkService));

}  // namespace

}  // namespace content
