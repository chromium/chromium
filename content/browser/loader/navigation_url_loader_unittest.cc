// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "ipc/ipc_message.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

class NavigationURLLoaderTest : public testing::Test {
 public:
  NavigationURLLoaderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext) {
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NavigationURLLoader> MakeTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    return CreateTestLoader(url, delegate);
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    mojom::BeginNavigationParamsPtr begin_params =
        mojom::BeginNavigationParams::New(
            MSG_ROUTING_NONE /* initiator_routing_id */,
            std::string() /* headers */, net::LOAD_NORMAL,
            false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            network::mojom::RequestDestination::kDocument,
            blink::WebMixedContentContextType::kBlockable,
            false /* is_form_submission */,
            false /* was_initiated_by_link_click */,
            GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            GURL() /* client_side_redirect_url */,
            base::nullopt /* devtools_initiator_info */,
            false /* force_ignore_site_for_cookies */,
            nullptr /* trust_token_params */, base::nullopt /* impression */,
            base::TimeTicks() /* renderer_before_unload_start */,
            base::TimeTicks() /* renderer_before_unload_end */);
    auto common_params = CreateCommonNavigationParams();
    common_params->url = url;
    common_params->initiator_origin = url::Origin::Create(url);

    url::Origin origin = url::Origin::Create(url);
    std::unique_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(
            std::move(common_params), std::move(begin_params),
            net::IsolationInfo::Create(
                net::IsolationInfo::RedirectMode::kUpdateTopFrame, origin,
                origin, net::SiteForCookies::FromUrl(url)),
            true /* is_main_frame */, false /* parent_is_main_frame */,
            false /* are_ancestors_secure */, -1 /* frame_tree_node_id */,
            false /* is_for_guests_only */, false /* report_raw_headers */,
            false /* is_prerendering */, false /* upgrade_if_insecure */,
            nullptr /* blob_url_loader_factory */,
            base::UnguessableToken::Create() /* devtools_navigation_token */,
            base::UnguessableToken::Create() /* devtools_frame_token */,
            false /* obey_origin_policy */, {} /* cors_exempt_headers */,
            nullptr /* client_security_state */));
    return NavigationURLLoader::Create(
        browser_context_.get(),
        BrowserContext::GetDefaultStoragePartition(browser_context_.get()),
        std::move(request_info), nullptr, nullptr, nullptr, nullptr, delegate,
        false, mojo::NullRemote());
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

// Tests that request failures are propagated correctly.
TEST_F(NavigationURLLoaderTest, RequestFailedNoCertError) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(GURL("bogus:bogus"), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_ABORTED, delegate.net_error());
  EXPECT_FALSE(delegate.ssl_info().is_valid());
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly for a (non-fatal) cert
// error:
// - |ssl_info| has the expected values.
TEST_F(NavigationURLLoaderTest, RequestFailedCertError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(https_server.GetURL("/"), &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, delegate.net_error());
  net::SSLInfo ssl_info = delegate.ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_TRUE(
      https_server.GetCertificate()->EqualsExcludingChain(ssl_info.cert.get()));
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(ssl_info.cert_status));
  EXPECT_FALSE(ssl_info.is_fatal_cert_error);
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

// Tests that request failures are propagated correctly for a fatal cert error:
// - |ssl_info| has the expected values.
TEST_F(NavigationURLLoaderTest, RequestFailedCertErrorFatal) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL("/");

  // Set HSTS for the test domain in order to make SSL errors fatal.
  base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
  bool include_subdomains = false;
  auto* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context_.get());
  base::RunLoop run_loop;
  storage_partition->GetNetworkContext()->AddHSTS(
      url.host(), expiry, include_subdomains, run_loop.QuitClosure());
  run_loop.Run();

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(url, &delegate);

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  ASSERT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, delegate.net_error());
  net::SSLInfo ssl_info = delegate.ssl_info();
  EXPECT_TRUE(ssl_info.is_valid());
  EXPECT_TRUE(
      https_server.GetCertificate()->EqualsExcludingChain(ssl_info.cert.get()));
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID,
            net::MapCertStatusToNetError(ssl_info.cert_status));
  EXPECT_TRUE(ssl_info.is_fatal_cert_error);
  EXPECT_EQ(1, delegate.on_request_handled_counter());
}

}  // namespace content
