// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/browser/loader/keep_alive_request_browsertest_util.h"
#include "content/browser/loader/keep_alive_url_loader_service.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

class SendBeaconBrowserTestBase : public KeepAliveRequestBrowserTestBase {
 protected:
  virtual std::string beacon_payload_type() const = 0;

  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}});
    return enabled_features;
  }

  GURL GetBeaconPageURL(
      const GURL& beacon_url,
      bool with_non_cors_safelisted_content,
      std::optional<int> delay_iframe_removal_ms = std::nullopt) const {
    std::vector<std::string> queries = {
        "/send-beacon-in-iframe.html?url=" + EncodeURL(beacon_url),
        "&payload_type=" + beacon_payload_type()};
    if (with_non_cors_safelisted_content) {
      // Setting the payload's content type to `application/octet-stream`, as
      // only `application/x-www-form-urlencoded`, `multipart/form-data`, and
      // `text/plain` MIME types are allowed for CORS-safelisted `content-type`
      // request header.
      // https://fetch.spec.whatwg.org/#cors-safelisted-request-header
      queries.push_back("&payload_content_type=application/octet-stream");
    }
    if (delay_iframe_removal_ms.has_value()) {
      queries.push_back(base::StringPrintf("&delay_iframe_removal_ms=%d",
                                           delay_iframe_removal_ms.value()));
    }

    return server()->GetURL(kPrimaryHost, base::StrCat(queries));
  }

  // Navigates to a page that calls `navigator.sendBeacon(beacon_url)` from a
  // programmatically created iframe. The iframe will then be removed after the
  // JS call after an optional `delay_iframe_removal_ms` interval.
  // `request_handler` must handle the final URL of the sendBeacon request.
  void LoadPageWithIframeAndSendBeacon(
      const GURL& beacon_url,
      net::test_server::ControllableHttpResponse* request_handler,
      const std::string& response,
      int expect_total_redirects,
      std::optional<int> delay_iframe_removal_ms = std::nullopt) {
    // Navigate to the page that calls sendBeacon with `beacon_url` from an
    // appended iframe.
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        GetBeaconPageURL(beacon_url,
                         /*with_non_cors_safelisted_content=*/false,
                         delay_iframe_removal_ms)));
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // All redirects, if exist, should be processed in browser first.
    loaders_observer().WaitForTotalOnReceiveRedirectProcessed(
        expect_total_redirects);
    // After in-browser processing, the loader should remain alive to support
    // forwarding stored redirects/response to renderer. But it may or may not
    // connect to a renderer.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // Ensure the sendBeacon request is sent.
    request_handler->WaitForRequest();
    // Send back final response to terminate in-browser request handling.
    request_handler->Send(response);
    request_handler->Done();

    // After in-browser redirect/response processing, the in-browser logic may
    // or may not forward redirect/response to renderer process, depending on
    // whether the renderer is still alive.
    loaders_observer().WaitForTotalOnReceiveResponse(1);
    // OnComplete may not be called if the renderer dies too early in before
    // receiving response.

    // The loader should all be gone.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  }
};

class SendBeaconBrowserTest
    : public SendBeaconBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  std::string beacon_payload_type() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SendBeaconBrowserTest,
    ::testing::Values("string", "arraybuffer", "form", "blob"),
    [](const testing::TestParamInfo<SendBeaconBrowserTest::ParamType>& info) {
      return info.param;
    });

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// The JS call happens in an iframe that is removed right after the sendBeacon()
// call, so the chain of redirects & response handling must survive the iframe
// unload.
// TODO(crbug.com/412499381): Re-enable this test.
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       DISABLED_MultipleRedirectsRequestWithIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// Unlike the `MultipleRedirectsRequestWithIframeRemoval` test case above, the
// request here is fired within an iframe that will be removed shortly
// (delayed by 0ms, roughly in the JS next event cycle).
// This is to mimic the following scenario:
//
// 1. The server returns a redirect.
// 2. In the browser process KeepAliveURLLoader::OnReceiveRedirect(),
//    forwarding_client_ is not null (as renderer/iframe still exists), so it
//    calls forwarding_client_->OnReceiveRedirect() IPC to forward to renderer.
// 3. The renderer process is somehow shut down before its
//    URLLoaderClient::OnReceiveRedirect() is finished, so the redirect chain is
//    incompleted.
// 4. KeepAliveURLLoader::OnRendererConnectionError() is triggered, and only
//    aware of forwarding_client_'s disconnection. It should take over redirect
//    chain handling.
//
// Without delaying iframe removal, renderer disconnection may happen in between
// (2) and (3).
// TODO(crbug.com/412499381): Re-enable this test.
IN_PROC_BROWSER_TEST_P(
    SendBeaconBrowserTest,
    DISABLED_MultipleRedirectsRequestWithDelayedIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3,
                                  /*delay_iframe_removal_ms=*/0);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should succeed.
// TODO(crbug.com/412499381): Re-enable this test.
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       DISABLED_CrossOriginAndCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.test:<port>/no-cors-server-redirect-307?...",
  //     <CORS-safelisted payload>)
  // --> http://b.test:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/1);
}

class SendBeaconBlobBrowserTest : public SendBeaconBrowserTestBase {
 protected:
  std::string beacon_payload_type() const override { return "blob"; }
};

// Tests navigator.sendBeacon() with a cross-origin & non-CORS-safelisted
// request that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should fail.
IN_PROC_BROWSER_TEST_F(SendBeaconBlobBrowserTest,
                       CrossOriginAndNonCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with non-CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.test:<port>/no-cors-server-redirect-307?...",
  //     <non-CORS-safelisted payload>) => should fail here
  // --> http://b.test:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));
  // Navigate to the page that calls sendBeacon with `beacon_url` from an
  // appended iframe, which will be removed shortly after calling sendBeacon().
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      GetBeaconPageURL(beacon_url, /*with_non_cors_safelisted_content=*/true)));

  // The redirect is rejected in-browser during redirect (with
  // non-CORS-safelisted payload) handling because /no-cors-server-redirect-xxx
  // doesn't support CORS. Thus, KeepAliveURLLoader::OnReceiveRedirect() is not
  // called but KeepAliveURLLoader::OnComplete().
  // Note that renderer can be gone at any point before or after the first URL
  // is loaded. So OnComplete() may or may not be forwarded.
  loaders_observer().WaitForTotalOnComplete({net::ERR_FAILED});
  EXPECT_FALSE(request_handler->has_received_request());
  // After in-browser processing, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  ExpectFetchKeepAliveHistogram(
      FetchKeepAliveRequestMetricType::kBeacon,
      ExpectedTotalRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedStartedRequests(/*browser=*/1, /*renderer=*/1),
      ExpectedSucceededRequests(/*browser=*/0, /*renderer=*/0),
      ExpectedFailedRequests(/*browser=*/1, /*renderer=*/1));
}

}  // namespace content
