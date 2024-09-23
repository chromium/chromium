// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "content/test/test_web_contents.h"
#include "ipc/ipc_message.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_response_headers.h"
#include "net/storage_access_api/status.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "url/origin.h"

namespace content {

class NavigationURLLoaderTest : public testing::Test {
 public:
  NavigationURLLoaderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        browser_context_(new TestBrowserContext) {
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    // Do not create TestNavigationURLLoaderFactory as this tests creates
    // NavigationURLLoaders explicitly and TestNavigationURLLoaderFactory
    // interferes with that.
    rvh_test_enabler_ = std::make_unique<RenderViewHostTestEnabler>(
        RenderViewHostTestEnabler::NavigationURLLoaderFactoryType::kNone);
    web_contents_ = TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get()));
    // NavigationURLLoader assumes that the corresponding FrameTreeNode has an
    // associated NavigationRequest.
    pending_navigation_ = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.com"), web_contents_.get());
    pending_navigation_->Start();
  }

  void TearDown() override {
    pending_navigation_.reset();
    web_contents_.reset();
  }

  std::unique_ptr<NavigationURLLoader> MakeTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    return CreateTestLoader(url, delegate);
  }

  std::unique_ptr<NavigationURLLoader> CreateTestLoader(
      const GURL& url,
      NavigationURLLoaderDelegate* delegate) {
    blink::mojom::BeginNavigationParamsPtr begin_params =
        blink::mojom::BeginNavigationParams::New(
            std::nullopt /* initiator_frame_token */,
            std::string() /* headers */, net::LOAD_NORMAL,
            false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            blink::mojom::MixedContentContextType::kBlockable,
            false /* is_form_submission */,
            false /* was_initiated_by_link_click */,
            blink::mojom::ForceHistoryPush::kNo,
            GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            GURL() /* client_side_redirect_url */,
            std::nullopt /* devtools_initiator_info */,
            nullptr /* trust_token_params */, std::nullopt /* impression */,
            base::TimeTicks() /* renderer_before_unload_start */,
            base::TimeTicks() /* renderer_before_unload_end */,
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kDidNotStartWithTransientActivation,
            false /* is_container_initiated */,
            net::StorageAccessApiStatus::kNone, false /* has_rel_opener */);
    auto common_params = blink::CreateCommonNavigationParams();
    common_params->url = url;
    common_params->initiator_origin = url::Origin::Create(url);
    common_params->request_destination =
        network::mojom::RequestDestination::kDocument;

    StoragePartition* storage_partition =
        browser_context_->GetDefaultStoragePartition();

    FrameTreeNodeId frame_tree_node_id =
        web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();

    url::Origin origin = url::Origin::Create(url);
    std::unique_ptr<NavigationRequestInfo> request_info(
        std::make_unique<NavigationRequestInfo>(
            std::move(common_params), std::move(begin_params),
            network::mojom::WebSandboxFlags::kNone,
            net::IsolationInfo::Create(
                net::IsolationInfo::RequestType::kMainFrame, origin, origin,
                net::SiteForCookies::FromUrl(url)),
            true /* is_primary_main_frame */,
            true /* is_outermost_main_frame */, true /* is_main_frame */,
            false /* are_ancestors_secure */, frame_tree_node_id,
            false /* report_raw_headers */, false /* upgrade_if_insecure */,
            nullptr /* blob_url_loader_factory */,
            base::UnguessableToken::Create() /* devtools_navigation_token */,
            base::UnguessableToken::Create() /* devtools_frame_token */,
            net::HttpRequestHeaders() /* cors_exempt_headers */,
            nullptr /* client_security_state */,
            std::nullopt /* devtools_accepted_stream_types */,
            false /* is_pdf */,
            ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
            std::nullopt /* initiator_document_token */,
            GlobalRenderFrameHostId() /* previous_render_frame_host_id */,
            nullptr /* serving_page_metrics_container */,
            false /* allow_cookies_from_browser */, 0 /* navigation_id */,
            false /* shared_storage_writable */, false /* is_ad_tagged */,
            false /* force_no_https_upgrade */));
    return NavigationURLLoader::Create(
        browser_context_.get(), storage_partition, std::move(request_info),
        nullptr, nullptr, nullptr, delegate,
        NavigationURLLoader::LoaderType::kRegular, mojo::NullRemote(),
        /* trust_token_observer=*/mojo::NullRemote(),
        /* shared_dictionary_observer=*/mojo::NullRemote(),
        /* url_loader_network_observer */ mojo::NullRemote(),
        /*devtools_observer=*/mojo::NullRemote());
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TestWebContents> web_contents_;
  // NavigationURLLoaderImpl relies on the existence of the
  // |frame_tree_node->navigation_request()|.
  std::unique_ptr<NavigationSimulator> pending_navigation_;
};

// Tests that request failures are propagated correctly.
TEST_F(NavigationURLLoaderTest, RequestFailedNoCertError) {
  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader =
      MakeTestLoader(GURL("bogus:bogus"), &delegate);
  loader->Start();

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
  loader->Start();

  // Wait for the request to fail as expected.
  delegate.WaitForRequestFailed();
  EXPECT_EQ(net::ERR_CERT_COMMON_NAME_INVALID, delegate.net_error());
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
  base::Time expiry = base::Time::Now() + base::Days(1000);
  bool include_subdomains = false;
  auto* storage_partition = browser_context_->GetDefaultStoragePartition();
  base::RunLoop run_loop;
  storage_partition->GetNetworkContext()->AddHSTS(
      url.host(), expiry, include_subdomains, run_loop.QuitClosure());
  run_loop.Run();

  TestNavigationURLLoaderDelegate delegate;
  std::unique_ptr<NavigationURLLoader> loader = MakeTestLoader(url, &delegate);
  loader->Start();

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
