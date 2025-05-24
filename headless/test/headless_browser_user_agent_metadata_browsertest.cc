// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace {

// Generate some fake UserAgentMetadata for use when overriding with CDP.
base::Value::Dict MakeFakeMetadata() {
  base::Value::Dict brand_version;
  brand_version.Set("brand", "HeadlessChrome");

  base::Value::List brands;
  brand_version.Set("version", "1");
  brands.Append(brand_version.Clone());

  brand_version.Set("version", "1.2.3");
  base::Value::List full_version_list;
  full_version_list.Append(std::move(brand_version));

  base::Value::Dict metadata;
  metadata.Set("brands", std::move(brands));
  metadata.Set("fullVersionList", std::move(full_version_list));
  metadata.Set("fullVersion", "1.2.3");
  metadata.Set("platform", "browsertest");
  metadata.Set("platformVersion", "9.8.7");
  metadata.Set("architecture", "x86_512");
  metadata.Set("model", "y=ax+b");
  metadata.Set("mobile", true);
  metadata.Set("bitness", "512");
  metadata.Set("wow64", true);

  return metadata;
}

}  // namespace

namespace headless {

class HeadlessBrowserNavigatorUADataTest : public HeadlessBrowserTest {
 public:

  void SetUpOnMainThread() override {
    HeadlessBrowserTest::SetUpOnMainThread();

    EXPECT_TRUE(embedded_test_server()->Start());

    HeadlessBrowserContext* browser_context =
        browser()->CreateBrowserContextBuilder().Build();

    web_contents_ =
        browser_context->CreateWebContentsBuilder()
            .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
            .Build();
    EXPECT_TRUE(WaitForLoad(web_contents_));

    devtools_client_.AttachToWebContents(
        HeadlessWebContentsImpl::From(web_contents_)->web_contents());
  }

  void TearDownOnMainThread() override {
    devtools_client_.DetachClient();
    HeadlessBrowserTest::TearDownOnMainThread();
  }

  // Get a UserAgentMetadata value as seen from navigator.userAgentData.
  base::Value::Dict GetUAMetadataValue(std::string script) {
    base::Value::Dict params = Param("expression", script);
    // Always await a Promise. This does not hurt if the script returns a
    // non-Promise value.
    params.Set("awaitPromise", true);
    return SendCommandSync(devtools_client_, "Runtime.evaluate",
                           std::move(params));
  }

  // Use the Chrome Devtools Protocol to override UserAgentMetadata.
  void OverrideUserAgentMetadata(base::Value::Dict user_agent_metadata) {
    base::Value::Dict params;
    params.Set("userAgent", "overridden");
    params.Set("userAgentMetadata",
               base::Value(std::move(user_agent_metadata)));
    base::Value::Dict result = SendCommandSync(
        devtools_client_, "Network.setUserAgentOverride", std::move(params));
    std::string* err = result.FindStringByDottedPath("error.data");
    CHECK(!err) << "Error invoking Network.setUserAgentOverride: \n" << *err;
  }

 protected:
  raw_ptr<HeadlessWebContents, AcrossTasksDanglingUntriaged> web_contents_;
  SimpleDevToolsProtocolClient devtools_client_;

  // Get the version of the HeadlessChrome brand from the brand list.
  static constexpr char kBrandVersionScript[] = R"(
      navigator.userAgentData.brands
          .find(({brand}) => brand == 'HeadlessChrome')
          .version)";
  static constexpr char kMobileScript[] = R"(navigator.userAgentData.mobile)";
  static constexpr char kPlatformScript[] =
      R"(navigator.userAgentData.platform)";
  static constexpr char kArchitectureScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['architecture'])
          .then(r => r.architecture))";
  static constexpr char kBitnessScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['bitness'])
          .then(r => r.bitness))";
  static constexpr char kModelScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['model'])
          .then(r => r.model))";
  static constexpr char kPlatformVersionScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['platformVersion'])
          .then(r => r.platformVersion))";
  static constexpr char kFullVersionScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['uaFullVersion'])
          .then(r => r.uaFullVersion))";
  // Get the version of the HeadlessChrome brand from fullVersionList.
  static constexpr char kFullVersionFromListScript[] = R"(
      navigator.userAgentData.getHighEntropyValues(['fullVersionList'])
          .then(({fullVersionList}) => fullVersionList
              .find(({brand}) => brand == 'HeadlessChrome')
              .version))";
  static constexpr char kWow64Script[] = R"(
          navigator.userAgentData.getHighEntropyValues(['wow64'])
              .then(r => r.wow64))";
  static constexpr char kFormFactorScript[] = R"(
          navigator.userAgentData.getHighEntropyValues(['formFactors'])
              .then(r => r.formFactors.join(', ')))";
};

// UA Metadata is available via `navigator.userAgentData`.
IN_PROC_BROWSER_TEST_F(HeadlessBrowserNavigatorUADataTest, DefaultValues) {
  auto expected = embedder_support::GetUserAgentMetadata();

  EXPECT_THAT(GetUAMetadataValue(kBrandVersionScript),
              DictHasValue("result.result.value",
                           version_info::GetMajorVersionNumber()));
  EXPECT_THAT(GetUAMetadataValue(kMobileScript),
              DictHasValue("result.result.value", expected.mobile));
  EXPECT_THAT(GetUAMetadataValue(kPlatformScript),
              DictHasValue("result.result.value", expected.platform));
  EXPECT_THAT(GetUAMetadataValue(kArchitectureScript),
              DictHasValue("result.result.value", expected.architecture));
  EXPECT_THAT(GetUAMetadataValue(kBitnessScript),
              DictHasValue("result.result.value", expected.bitness));
  EXPECT_THAT(GetUAMetadataValue(kModelScript),
              DictHasValue("result.result.value", expected.model));
  EXPECT_THAT(GetUAMetadataValue(kPlatformVersionScript),
              DictHasValue("result.result.value", expected.platform_version));
  EXPECT_THAT(GetUAMetadataValue(kFullVersionScript),
              DictHasValue("result.result.value", expected.full_version));
  EXPECT_THAT(GetUAMetadataValue(kFullVersionFromListScript),
              DictHasValue("result.result.value", expected.full_version));
  EXPECT_THAT(GetUAMetadataValue(kWow64Script),
              DictHasValue("result.result.value", expected.wow64));
  EXPECT_THAT(GetUAMetadataValue(kFormFactorScript),
              DictHasValue("result.result.value",
                           base::JoinString(expected.form_factors, ", ")));
}

// UA Metadata is available via `navigator.userAgentData` when overridden via
// Devtools.
IN_PROC_BROWSER_TEST_F(HeadlessBrowserNavigatorUADataTest, CDPOverride) {
  OverrideUserAgentMetadata(MakeFakeMetadata());

  EXPECT_THAT(GetUAMetadataValue(kBrandVersionScript),
              DictHasValue("result.result.value", "1"));
  EXPECT_THAT(GetUAMetadataValue(kMobileScript),
              DictHasValue("result.result.value", true));
  EXPECT_THAT(GetUAMetadataValue(kPlatformScript),
              DictHasValue("result.result.value", "browsertest"));
  EXPECT_THAT(GetUAMetadataValue(kArchitectureScript),
              DictHasValue("result.result.value", "x86_512"));
  EXPECT_THAT(GetUAMetadataValue(kBitnessScript),
              DictHasValue("result.result.value", "512"));
  EXPECT_THAT(GetUAMetadataValue(kModelScript),
              DictHasValue("result.result.value", "y=ax+b"));
  EXPECT_THAT(GetUAMetadataValue(kPlatformVersionScript),
              DictHasValue("result.result.value", "9.8.7"));
  EXPECT_THAT(GetUAMetadataValue(kFullVersionScript),
              DictHasValue("result.result.value", "1.2.3"));
  EXPECT_THAT(GetUAMetadataValue(kFullVersionFromListScript),
              DictHasValue("result.result.value", "1.2.3"));
  EXPECT_THAT(GetUAMetadataValue(kWow64Script),
              DictHasValue("result.result.value", true));
  // TODO(crbug.com/40910451): Allow overriding formFactors.
  EXPECT_THAT(GetUAMetadataValue(kFormFactorScript),
              DictHasValue("result.result.value", ""));
}

class HeadlessBrowserUAHeaderTest : public HeadlessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    HeadlessBrowserTest::SetUpOnMainThread();

    EXPECT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &HeadlessBrowserUAHeaderTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    HeadlessBrowserContext* browser_context =
        browser()->CreateBrowserContextBuilder().Build();

    // Capture the initial request.
    CaptureHeadersForPath("/hello.html");

    web_contents_ =
        browser_context->CreateWebContentsBuilder()
            .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
            .Build();
    EXPECT_TRUE(WaitForLoad(web_contents_));

    devtools_client_.AttachToWebContents(
        HeadlessWebContentsImpl::From(web_contents_)->web_contents());
  }

  void TearDownOnMainThread() override {
    devtools_client_.DetachClient();
    HeadlessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    auto path = request.GetURL().path();
    if (path == capture_headers_for_path_) {
      got_headers_ = request.headers;
    }

    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");

    if (path == "/hello.html") {
      // Request all CH's, and include a cross-site (sandboxed) iframe, on
      // fetches of "hello.html". This includes deprecated "RTT" and "Downlink"
      // headers as well.
      http_response->AddCustomHeader(
          "Accept-CH",
          "Sec-CH-UA-Arch, Sec-CH-UA-Bitness, Sec-CH-UA-Full-Version, "
          "Sec-CH-UA-Full-Version-List, Sec-CH-UA-Mobile, Sec-CH-UA-Model, "
          "Sec-CH-UA-Platform-Version, Sec-CH-UA-Platform, Sec-CH-UA-Wow64, "
          "Sec-CH-UA, RTT, Downlink");
      http_response->set_content(
          "<html><iframe sandbox src='/iframe.html'></iframe></html>");
    } else {
      // All other paths (including `/iframe.html`) are just a boring document
      // without Accept-CH.
      http_response->set_content("<html></html>");
    }

    return http_response;
  }

  void CaptureHeadersForPath(const std::string path) {
    capture_headers_for_path_ = path;
  }

  bool IsRequestHeaderSet(
      const std::string header,
      const std::optional<std::string> value = std::nullopt) {
    if (!got_headers_.contains(header)) {
      return false;
    }

    return value.has_value() ? got_headers_[header] == *value
                             // Check that the header value is not empty.
                             : !got_headers_[header].empty();
  }

  // Use the Chrome Devtools Protocol to override UserAgentMetadata.
  void OverrideUserAgentMetadata(base::Value::Dict user_agent_metadata) {
    base::Value::Dict params;
    params.Set("userAgent", "overridden");
    params.Set("userAgentMetadata",
               base::Value(std::move(user_agent_metadata)));
    base::Value::Dict result = SendCommandSync(
        devtools_client_, "Network.setUserAgentOverride", std::move(params));
    std::string* err = result.FindStringByDottedPath("error.data");
    CHECK(!err) << "Error invoking Network.setUserAgentOverride: \n" << *err;
  }

  // Use the Chrome Devtools Protocol to reload the page, ignoring cache, and
  // wait for the load to complete.
  void ReloadPage() {
    base::Value::Dict params;
    params.Set("ignoreCache", true);
    base::Value::Dict result =
        SendCommandSync(devtools_client_, "Page.reload", std::move(params));
    std::string* err = result.FindStringByDottedPath("error.data");
    CHECK(!err) << "Error invoking Page.reload: \n" << *err;
    EXPECT_TRUE(WaitForLoad(web_contents_));
  }

  // Use the fetch API to make a subresource request.
  base::Value::Dict FetchSubresource() {
    base::Value::Dict params = Param("expression", "fetch('/sub')");
    params.Set("awaitPromise", true);
    return SendCommandSync(devtools_client_, "Runtime.evaluate",
                           std::move(params));
  }

  // Return the boolean header forms `?0` and `?1`.
  std::string BoolHeader(bool value) { return value ? "?1" : "?0"; }

  // Return the simple string header form, simply surrounding with `"`
  // characters.
  std::string StringHeader(std::string value) {
    return base::StrCat({"\"", value, "\""});
  }

  // Check that the request headers contain the default values as if no
  // Accept-CH was sent.
  void ExpectDefaultValues() {
    auto expected = embedder_support::GetUserAgentMetadata();

    // Only sec-ch-ua, mobile, and platform are set with no Accept-CH.
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua"));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-mobile", BoolHeader(expected.mobile)));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-full-version"));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-full-version-list"));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-platform",
                                   StringHeader(expected.platform)));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-platform-version"));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-arch"));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-wow64"));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-bitness"));
    EXPECT_FALSE(IsRequestHeaderSet("sec-ch-ua-model"));
    EXPECT_FALSE(IsRequestHeaderSet("rtt"));
    EXPECT_FALSE(IsRequestHeaderSet("downlink"));
  }

  // Check that the request headers included the values from embedder_support.
  void ExpectEmbedderSupportValues() {
    auto expected = embedder_support::GetUserAgentMetadata();

    // To avoid parsing the greased version lists, this only checks that they
    // are present.
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua"));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-mobile", BoolHeader(expected.mobile)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-full-version",
                                   StringHeader(expected.full_version)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-full-version-list"));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-platform",
                                   StringHeader(expected.platform)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-platform-version",
                                   StringHeader(expected.platform_version)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-arch",
                                   StringHeader(expected.architecture)));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-wow64", BoolHeader(expected.wow64)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-bitness",
                                   StringHeader(expected.bitness)));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-model", StringHeader(expected.model)));
    EXPECT_FALSE(IsRequestHeaderSet("rtt"));
    EXPECT_FALSE(IsRequestHeaderSet("downlink"));
  }

  // Check that the request headers included the values from
  // OverrideUserAgentMetadata.
  void ExpectOverriddenValues() {
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua"));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-mobile", BoolHeader(true)));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-full-version", StringHeader("1.2.3")));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-full-version-list"));
    EXPECT_TRUE(
        IsRequestHeaderSet("sec-ch-ua-platform", StringHeader("browsertest")));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-platform-version",
                                   StringHeader("9.8.7")));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-arch", StringHeader("x86_512")));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-wow64", BoolHeader(true)));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-bitness", StringHeader("512")));
    EXPECT_TRUE(IsRequestHeaderSet("sec-ch-ua-model", StringHeader("y=ax+b")));
    EXPECT_FALSE(IsRequestHeaderSet("rtt"));
    EXPECT_FALSE(IsRequestHeaderSet("downlink"));
  }

 protected:
  raw_ptr<HeadlessWebContents, AcrossTasksDanglingUntriaged> web_contents_;
  SimpleDevToolsProtocolClient devtools_client_;
  // HandleRequest will capture headers with this path in `got_headers_`.
  std::string capture_headers_for_path_;
  // Captured headers from the last request to `capture_headers_for_path_`.
  HttpRequest::HeaderMap got_headers_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, OnInitialNavigation) {
  // The initial request has already been captured in test setup.
  ExpectDefaultValues();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, OnSubsequentNavigation) {
  CaptureHeadersForPath("/hello.html");
  // Reload so that the Accept-CH in the initial request takes effect.
  ReloadPage();
  ExpectEmbedderSupportValues();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, OnThirdPartySubframe) {
  CaptureHeadersForPath("/iframe.html");
  ReloadPage();
  ExpectDefaultValues();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, CDPOverride) {
  CaptureHeadersForPath("/hello.html");
  OverrideUserAgentMetadata(MakeFakeMetadata());
  // Reload so that the Accept-CH in the initial request takes effect.
  ReloadPage();
  ExpectOverriddenValues();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, Subresource) {
  CaptureHeadersForPath("/sub");
  FetchSubresource();
  ExpectEmbedderSupportValues();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserUAHeaderTest, SubresourceCDPOverride) {
  CaptureHeadersForPath("/sub");
  OverrideUserAgentMetadata(MakeFakeMetadata());
  FetchSubresource();
  ExpectOverriddenValues();
}

}  // namespace headless
