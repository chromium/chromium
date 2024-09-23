/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using blink::test::RunPendingTasks;
using blink::url_test_helpers::ToKURL;

namespace blink {

class WebAssociatedURLLoaderTest : public testing::Test,
                                   public WebAssociatedURLLoaderClient {
 public:
  WebAssociatedURLLoaderTest()
      : will_follow_redirect_(false),
        did_send_data_(false),
        did_receive_response_(false),
        did_receive_data_(false),
        did_finish_loading_(false),
        did_fail_(false) {
    // Reuse one of the test files from WebFrameTest.
    frame_file_path_ = test::CoreTestDataPath("iframes_test.html");
  }

  void RegisterMockedURLLoadWithCustomResponse(const WebURL& full_url,
                                               WebURLResponse response,
                                               const WebString& file_path) {
    url_test_helpers::RegisterMockedURLLoadWithCustomResponse(
        full_url, file_path, response);
  }

  KURL RegisterMockedUrl(const std::string& url_root,
                         const WTF::String& filename) {
    WebURLResponse response;
    response.SetMimeType("text/html");
    KURL url = ToKURL(url_root + filename.Utf8());
    RegisterMockedURLLoadWithCustomResponse(
        url, response, test::CoreTestDataPath(filename.Utf8().c_str()));
    return url;
  }

  void SetUp() override {
    helper_.Initialize();

    std::string url_root = "http://www.test.com/";
    KURL url = RegisterMockedUrl(url_root, "iframes_test.html");
    const char* iframe_support_files[] = {
        "invisible_iframe.html",
        "visible_iframe.html",
        "zero_sized_iframe.html",
    };
    for (size_t i = 0; i < std::size(iframe_support_files); ++i) {
      RegisterMockedUrl(url_root, iframe_support_files[i]);
    }

    frame_test_helpers::LoadFrame(MainFrame(), url.GetString().Utf8().c_str());

    url_test_helpers::RegisterMockedURLUnregister(url);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void ServeRequests() { url_test_helpers::ServeAsynchronousRequests(); }

  std::unique_ptr<WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions options =
          WebAssociatedURLLoaderOptions()) {
    return MainFrame()->CreateAssociatedURLLoader(options);
  }

  // WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(const WebURL& new_url,
                          const WebURLResponse& redirect_response) override {
    will_follow_redirect_ = true;
    EXPECT_EQ(expected_new_url_, new_url);
    EXPECT_EQ(expected_redirect_response_.CurrentRequestUrl(),
              redirect_response.CurrentRequestUrl());
    EXPECT_EQ(expected_redirect_response_.HttpStatusCode(),
              redirect_response.HttpStatusCode());
    EXPECT_EQ(expected_redirect_response_.MimeType(),
              redirect_response.MimeType());
    return true;
  }

  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override {
    did_send_data_ = true;
  }

  void DidReceiveResponse(const WebURLResponse& response) override {
    did_receive_response_ = true;
    actual_response_ = WebURLResponse(response);
    EXPECT_EQ(expected_response_.CurrentRequestUrl(),
              response.CurrentRequestUrl());
    EXPECT_EQ(expected_response_.HttpStatusCode(), response.HttpStatusCode());
  }

  void DidDownloadData(uint64_t data_length) override {
    did_download_data_ = true;
  }

  void DidReceiveData(base::span<const char> data) override {
    did_receive_data_ = true;
    EXPECT_TRUE(data.data());
    EXPECT_GT(data.size(), 0u);
  }

  void DidFinishLoading() override { did_finish_loading_ = true; }

  void DidFail(const WebURLError& error) override { did_fail_ = true; }

  void CheckMethodFails(const char* unsafe_method) {
    WebURLRequest request(ToKURL("http://www.test.com/success.html"));
    request.SetMode(network::mojom::RequestMode::kSameOrigin);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
    request.SetHttpMethod(WebString::FromUTF8(unsafe_method));
    WebAssociatedURLLoaderOptions options;
    options.untrusted_http = true;
    CheckFails(request, options);
  }

  void CheckHeaderFails(const char* header_field) {
    CheckHeaderFails(header_field, "foo");
  }

  void CheckHeaderFails(const char* header_field, const char* header_value) {
    WebURLRequest request(ToKURL("http://www.test.com/success.html"));
    request.SetMode(network::mojom::RequestMode::kSameOrigin);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
    if (EqualIgnoringASCIICase(WebString::FromUTF8(header_field), "referer")) {
      request.SetReferrerString(WebString::FromUTF8(header_value));
      request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);
    } else {
      request.SetHttpHeaderField(WebString::FromUTF8(header_field),
                                 WebString::FromUTF8(header_value));
    }

    WebAssociatedURLLoaderOptions options;
    options.untrusted_http = true;
    CheckFails(request, options);
  }

  void CheckFails(
      const WebURLRequest& request,
      WebAssociatedURLLoaderOptions options = WebAssociatedURLLoaderOptions()) {
    expected_loader_ = CreateAssociatedURLLoader(options);
    EXPECT_TRUE(expected_loader_);
    did_fail_ = false;
    expected_loader_->LoadAsynchronously(request, this);
    // Failure should not be reported synchronously.
    EXPECT_FALSE(did_fail_);
    // Allow the loader to return the error.
    RunPendingTasks();
    EXPECT_TRUE(did_fail_);
    EXPECT_FALSE(did_receive_response_);
  }

  bool CheckAccessControlHeaders(const char* header_name, bool exposed) {
    std::string id("http://www.other.com/CheckAccessControlExposeHeaders_");
    id.append(header_name);
    if (exposed)
      id.append("-Exposed");
    id.append(".html");

    KURL url = ToKURL(id);
    WebURLRequest request(url);
    request.SetMode(network::mojom::RequestMode::kCors);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

    WebString header_name_string(WebString::FromUTF8(header_name));
    expected_response_ = WebURLResponse();
    expected_response_.SetMimeType("text/html");
    expected_response_.SetHttpStatusCode(200);
    expected_response_.AddHttpHeaderField("Access-Control-Allow-Origin", "*");
    if (exposed) {
      expected_response_.AddHttpHeaderField("access-control-expose-headers",
                                            header_name_string);
    }
    expected_response_.AddHttpHeaderField(header_name_string, "foo");
    RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                            frame_file_path_);

    WebAssociatedURLLoaderOptions options;
    expected_loader_ = CreateAssociatedURLLoader(options);
    EXPECT_TRUE(expected_loader_);
    expected_loader_->LoadAsynchronously(request, this);
    ServeRequests();
    EXPECT_TRUE(did_receive_response_);
    EXPECT_TRUE(did_receive_data_);
    EXPECT_TRUE(did_finish_loading_);

    return !actual_response_.HttpHeaderField(header_name_string).IsEmpty();
  }

  WebLocalFrameImpl* MainFrame() const {
    return helper_.GetWebView()->MainFrameImpl();
  }

 protected:
  test::TaskEnvironment task_environment_;
  String frame_file_path_;
  frame_test_helpers::WebViewHelper helper_;

  std::unique_ptr<WebAssociatedURLLoader> expected_loader_;
  WebURLResponse actual_response_;
  WebURLResponse expected_response_;
  WebURL expected_new_url_;
  WebURLResponse expected_redirect_response_;
  bool will_follow_redirect_;
  bool did_send_data_;
  bool did_receive_response_;
  bool did_download_data_;
  bool did_receive_data_;
  bool did_finish_loading_;
  bool did_fail_;
};

// Test a successful same-origin URL load.
TEST_F(WebAssociatedURLLoaderTest, SameOriginSuccess) {
  KURL url = ToKURL("http://www.test.com/SameOriginSuccess.html");
  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kSameOrigin);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                          frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test that the same-origin restriction is the default.
TEST_F(WebAssociatedURLLoaderTest, SameOriginRestriction) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url = ToKURL("http://www.other.com/SameOriginRestriction.html");
  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kSameOrigin);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  CheckFails(request);
}

// Test a successful cross-origin load.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginSuccess) {
  // This is cross-origin since the frame was loaded from www.test.com.
  KURL url = ToKURL("http://www.other.com/CrossOriginSuccess");
  WebURLRequest request(url);
  // No-CORS requests (CrossOriginRequestPolicyAllow) aren't allowed for the
  // default context. So we set the context as Script here.
  request.SetRequestContext(mojom::blink::RequestContextType::SCRIPT);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                          frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test a same-origin URL redirect and load.
TEST_F(WebAssociatedURLLoaderTest, RedirectSuccess) {
  KURL url = ToKURL("http://www.test.com/RedirectSuccess.html");
  char redirect[] = "http://www.test.com/RedirectSuccess2.html";  // Same-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kSameOrigin);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMimeType("text/html");
  expected_redirect_response_.SetHttpStatusCode(301);
  expected_redirect_response_.SetHttpHeaderField("Location", redirect);
  RegisterMockedURLLoadWithCustomResponse(url, expected_redirect_response_,
                                          frame_file_path_);

  expected_new_url_ = WebURL(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(redirect_url, expected_response_,
                                          frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(will_follow_redirect_);
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test a cross-origin URL redirect without Access Control set.
TEST_F(WebAssociatedURLLoaderTest, RedirectCrossOriginFailure) {
  KURL url = ToKURL("http://www.test.com/RedirectCrossOriginFailure.html");
  char redirect[] =
      "http://www.other.com/RedirectCrossOriginFailure.html";  // Cross-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kSameOrigin);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMimeType("text/html");
  expected_redirect_response_.SetHttpStatusCode(301);
  expected_redirect_response_.SetHttpHeaderField("Location", redirect);
  RegisterMockedURLLoadWithCustomResponse(url, expected_redirect_response_,
                                          frame_file_path_);

  expected_new_url_ = WebURL(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(redirect_url, expected_response_,
                                          frame_file_path_);

  expected_loader_ = CreateAssociatedURLLoader();
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);

  ServeRequests();
  EXPECT_FALSE(will_follow_redirect_);
  EXPECT_FALSE(did_receive_response_);
  EXPECT_FALSE(did_receive_data_);
  EXPECT_FALSE(did_finish_loading_);
}

// Test that a cross origin redirect response with CORS headers that allow the
// requesting origin succeeds.
TEST_F(WebAssociatedURLLoaderTest,
       RedirectCrossOriginWithAccessControlSuccess) {
  KURL url = ToKURL(
      "http://www.test.com/RedirectCrossOriginWithAccessControlSuccess.html");
  char redirect[] =
      "http://www.other.com/"
      "RedirectCrossOriginWithAccessControlSuccess.html";  // Cross-origin
  KURL redirect_url = ToKURL(redirect);

  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kCors);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  // Add a CORS simple header.
  request.SetHttpHeaderField("accept", "application/json");

  // Create a redirect response that allows the redirect to pass the access
  // control checks.
  expected_redirect_response_ = WebURLResponse();
  expected_redirect_response_.SetMimeType("text/html");
  expected_redirect_response_.SetHttpStatusCode(301);
  expected_redirect_response_.SetHttpHeaderField("Location", redirect);
  expected_redirect_response_.AddHttpHeaderField("access-control-allow-origin",
                                                 "*");
  RegisterMockedURLLoadWithCustomResponse(url, expected_redirect_response_,
                                          frame_file_path_);

  expected_new_url_ = WebURL(redirect_url);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  expected_response_.AddHttpHeaderField("access-control-allow-origin", "*");
  RegisterMockedURLLoadWithCustomResponse(redirect_url, expected_response_,
                                          frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(will_follow_redirect_);
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
}

// Test that untrusted loads can't use a forbidden method.
TEST_F(WebAssociatedURLLoaderTest, UntrustedCheckMethods) {
  // Check non-token method fails.
  CheckMethodFails("GET()");
  CheckMethodFails("POST\x0d\x0ax-csrf-token:\x20test1234");

  // Forbidden methods should fail regardless of casing.
  CheckMethodFails("CoNneCt");
  CheckMethodFails("TrAcK");
  CheckMethodFails("TrAcE");
}

// This test is flaky on Windows and Android. See <http://crbug.com/471645>.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#define MAYBE_UntrustedCheckHeaders DISABLED_UntrustedCheckHeaders
#else
#define MAYBE_UntrustedCheckHeaders UntrustedCheckHeaders
#endif

// Test that untrusted loads can't use a forbidden header field.
TEST_F(WebAssociatedURLLoaderTest, MAYBE_UntrustedCheckHeaders) {
  // Check non-token header fails.
  CheckHeaderFails("foo()");

  // Check forbidden headers fail.
  CheckHeaderFails("accept-charset");
  CheckHeaderFails("accept-encoding");
  CheckHeaderFails("connection");
  CheckHeaderFails("content-length");
  CheckHeaderFails("cookie");
  CheckHeaderFails("cookie2");
  CheckHeaderFails("date");
  CheckHeaderFails("dnt");
  CheckHeaderFails("expect");
  CheckHeaderFails("host");
  CheckHeaderFails("keep-alive");
  CheckHeaderFails("origin");
  CheckHeaderFails("referer", "http://example.com/");
  CheckHeaderFails("referer", "");  // no-referrer.
  CheckHeaderFails("te");
  CheckHeaderFails("trailer");
  CheckHeaderFails("transfer-encoding");
  CheckHeaderFails("upgrade");
  CheckHeaderFails("user-agent");
  CheckHeaderFails("via");

  CheckHeaderFails("proxy-");
  CheckHeaderFails("proxy-foo");
  CheckHeaderFails("sec-");
  CheckHeaderFails("sec-foo");

  // Check that validation is case-insensitive.
  CheckHeaderFails("AcCePt-ChArSeT");
  CheckHeaderFails("ProXy-FoO");
}

// Test that the loader filters response headers according to the CORS standard.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginHeaderSafelisting) {
  // Test that safelisted headers are returned without exposing them.
  EXPECT_TRUE(CheckAccessControlHeaders("cache-control", false));
  EXPECT_TRUE(CheckAccessControlHeaders("content-language", false));
  EXPECT_TRUE(CheckAccessControlHeaders("content-type", false));
  EXPECT_TRUE(CheckAccessControlHeaders("expires", false));
  EXPECT_TRUE(CheckAccessControlHeaders("last-modified", false));
  EXPECT_TRUE(CheckAccessControlHeaders("pragma", false));

  // Test that non-safelisted headers aren't returned.
  EXPECT_FALSE(CheckAccessControlHeaders("non-safelisted", false));

  // Test that Set-Cookie headers aren't returned.
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie", false));
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie2", false));

  // Test that exposed headers that aren't safelisted are returned.
  EXPECT_TRUE(CheckAccessControlHeaders("non-safelisted", true));

  // Test that Set-Cookie headers aren't returned, even if exposed.
  EXPECT_FALSE(CheckAccessControlHeaders("Set-Cookie", true));
}

// Test that the loader can allow non-safelisted response headers for trusted
// CORS loads.
TEST_F(WebAssociatedURLLoaderTest, CrossOriginHeaderAllowResponseHeaders) {
  KURL url =
      ToKURL("http://www.other.com/CrossOriginHeaderAllowResponseHeaders.html");
  WebURLRequest request(url);
  request.SetMode(network::mojom::RequestMode::kCors);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  WebString header_name_string(WebString::FromUTF8("non-safelisted"));
  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/html");
  expected_response_.SetHttpStatusCode(200);
  expected_response_.AddHttpHeaderField("Access-Control-Allow-Origin", "*");
  expected_response_.AddHttpHeaderField(header_name_string, "foo");
  RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                          frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  // This turns off response safelisting.
  options.expose_all_response_headers = true;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);

  EXPECT_FALSE(actual_response_.HttpHeaderField(header_name_string).IsEmpty());
}

TEST_F(WebAssociatedURLLoaderTest, AccessCheckForLocalURL) {
  KURL url = ToKURL("file://test.pdf");

  WebURLRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::PLUGIN);
  request.SetMode(network::mojom::RequestMode::kNoCors);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/plain");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                          frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();

  // The request failes due to a security check.
  EXPECT_FALSE(did_receive_response_);
  EXPECT_FALSE(did_receive_data_);
  EXPECT_FALSE(did_finish_loading_);
  EXPECT_TRUE(did_fail_);
}

TEST_F(WebAssociatedURLLoaderTest, BypassAccessCheckForLocalURL) {
  KURL url = ToKURL("file://test.pdf");

  WebURLRequest request(url);
  request.SetRequestContext(mojom::blink::RequestContextType::PLUGIN);
  request.SetMode(network::mojom::RequestMode::kNoCors);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);

  expected_response_ = WebURLResponse();
  expected_response_.SetMimeType("text/plain");
  expected_response_.SetHttpStatusCode(200);
  RegisterMockedURLLoadWithCustomResponse(url, expected_response_,
                                          frame_file_path_);

  WebAssociatedURLLoaderOptions options;
  options.grant_universal_access = true;
  expected_loader_ = CreateAssociatedURLLoader(options);
  EXPECT_TRUE(expected_loader_);
  expected_loader_->LoadAsynchronously(request, this);
  ServeRequests();

  // The security check is bypassed due to |grant_universal_access|.
  EXPECT_TRUE(did_receive_response_);
  EXPECT_TRUE(did_receive_data_);
  EXPECT_TRUE(did_finish_loading_);
  EXPECT_FALSE(did_fail_);
}

#undef MAYBE_UntrustedCheckHeaders
}  // namespace blink
