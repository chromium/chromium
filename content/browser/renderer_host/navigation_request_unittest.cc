// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

// Test version of a NavigationThrottle that will execute a callback when
// called.
class DeletingNavigationThrottle : public NavigationThrottle {
 public:
  DeletingNavigationThrottle(NavigationHandle* handle,
                             const base::RepeatingClosure& deletion_callback)
      : NavigationThrottle(handle), deletion_callback_(deletion_callback) {}
  ~DeletingNavigationThrottle() override = default;

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "DeletingNavigationThrottle";
  }

 private:
  base::RepeatingClosure deletion_callback_;
};

class NavigationRequestTest : public RenderViewHostImplTestHarness {
 public:
  NavigationRequestTest() : callback_result_(NavigationThrottle::DEFER) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Release the |request_| before destroying the WebContents, to match
    // the WebContentsObserverConsistencyChecker expectations.
    request_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) {
    request_->CancelDeferredNavigationInternal(result);
  }

  // Helper function to call WillStartRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of
  // the throttle checks when they are finished.
  void SimulateWillStartRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    request_->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    request_->WillStartRequest();
  }

  // Helper function to call WillRedirectRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillRedirectRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    request_->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    request_->WillRedirectRequest(
        GURL(), CoopCoepCrossOriginIsolatedInfo::CreateNonIsolated(),
        nullptr /* post_redirect_process */);
  }

  // Helper function to call WillFailRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  void SimulateWillFailRequest(
      net::Error net_error_code,
      const base::Optional<net::SSLInfo> ssl_info = base::nullopt) {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;
    request_->set_net_error(net_error_code);

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    request_->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    request_->WillFailRequest();
  }

  // Whether the callback was called.
  bool was_callback_called() const { return was_callback_called_; }

  // Returns the callback_result.
  NavigationThrottle::ThrottleCheckResult callback_result() const {
    return callback_result_;
  }

  NavigationRequest::NavigationState state() { return request_->state(); }

  bool call_counts_match(TestNavigationThrottle* throttle,
                         int start,
                         int redirect,
                         int failure,
                         int process) {
    return start == throttle->GetCallCount(
                        TestNavigationThrottle::WILL_START_REQUEST) &&
           redirect == throttle->GetCallCount(
                           TestNavigationThrottle::WILL_REDIRECT_REQUEST) &&
           failure == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_FAIL_REQUEST) &&
           process == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_PROCESS_RESPONSE);
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on checks by default.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        new TestNavigationThrottle(request_.get());
    test_throttle->SetResponseForAllMethods(TestNavigationThrottle::SYNCHRONOUS,
                                            result);
    request_->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(test_throttle));
    return test_throttle;
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on check for the given |method|, and
  // NavigationThrottle::PROCEED otherwise.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      TestNavigationThrottle::ThrottleMethod method,
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
    test_throttle->SetResponse(method, TestNavigationThrottle::SYNCHRONOUS,
                               result);
    return test_throttle;
  }

  // TODO(zetamoo): Use NavigationSimulator instead of creating
  // NavigationRequest and NavigationHandleImpl.
  void CreateNavigationHandle() {
    auto common_params = CreateCommonNavigationParams();
    common_params->initiator_origin =
        url::Origin::Create(GURL("https://initiator.example.com"));
    auto commit_params = CreateCommitNavigationParams();
    commit_params->frame_policy =
        main_test_rfh()->frame_tree_node()->pending_frame_policy();
    request_ = NavigationRequest::CreateBrowserInitiated(
        main_test_rfh()->frame_tree_node(), std::move(common_params),
        std::move(commit_params), false /* browser-initiated */,
        false /* was_opener_suppressed */, nullptr /* initiator_frame_token */,
        ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
        std::string() /* extra_headers */, nullptr /* frame_entry */,
        nullptr /* entry */, nullptr /* post_body */,
        nullptr /* navigation_ui_data */, base::nullopt /* impression */);
    request_->StartNavigation(true);
  }

 private:
  // The callback provided to NavigationRequest::WillStartRequest,
  // NavigationRequest::WillRedirectRequest, and
  // NavigationRequest::WillFailRequest during the tests.
  bool UpdateThrottleCheckResult(
      NavigationThrottle::ThrottleCheckResult result) {
    callback_result_ = result;
    was_callback_called_ = true;
    return true;
  }

  std::unique_ptr<NavigationRequest> request_;
  bool was_callback_called_ = false;
  NavigationThrottle::ThrottleCheckResult callback_result_;
};

// Checks that the request_context_type is properly set.
// Note: can be extended to cover more internal members.
TEST_F(NavigationRequestTest, SimpleDataChecksRedirectAndProcess) {
  const GURL kUrl1 = GURL("http://chromium.org");
  const GURL kUrl2 = GURL("http://google.com");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl1, main_rfh());
  navigation->Start();
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(
      net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1);
  navigation->Redirect(kUrl2);
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(
      net::HttpResponseInfo::CONNECTION_INFO_QUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());
}

TEST_F(NavigationRequestTest, SimpleDataCheckNoRedirect) {
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->Start();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(
      net::HttpResponseInfo::CONNECTION_INFO_QUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());
}

TEST_F(NavigationRequestTest, SimpleDataChecksFailure) {
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->Start();
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->Fail(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(blink::mojom::RequestContextType::HYPERLINK,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::ERR_CERT_DATE_INVALID,
            navigation->GetNavigationHandle()->GetNetErrorCode());
}

// Checks that a navigation deferred during WillStartRequest can be properly
// cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillStart) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));
}

// Checks that a navigation deferred during WillRedirectRequest can be properly
// cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillRedirect) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_EQ(NavigationRequest::WILL_REDIRECT_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0));
}

// Checks that a navigation deferred during WillFailRequest can be properly
// cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillFail) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationRequest::WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));
}

// Checks that a navigation deferred can be canceled and not ignored.
TEST_F(NavigationRequestTest, CancelDeferredWillRedirectNoIgnore) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));
}

// Checks that a navigation deferred by WillFailRequest can be canceled and not
// ignored.
TEST_F(NavigationRequestTest, CancelDeferredWillFailNoIgnore) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationRequest::WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0));
}

// Checks that data from the SSLInfo passed into SimulateWillStartRequest() is
// stored on the handle.
TEST_F(NavigationRequestTest, WillFailRequestSetsSSLInfo) {
  uint16_t cipher_suite = 0xc02f;  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  int connection_status = 0;
  net::SSLConnectionStatusSetCipherSuite(cipher_suite, &connection_status);

  // Set some test values.
  net::SSLInfo ssl_info;
  ssl_info.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  ssl_info.connection_status = connection_status;

  const GURL kUrl = GURL("https://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->SetSSLInfo(ssl_info);
  navigation->Fail(net::ERR_CERT_DATE_INVALID);

  EXPECT_EQ(net::CERT_STATUS_AUTHORITY_INVALID,
            navigation->GetNavigationHandle()->GetSSLInfo()->cert_status);
  EXPECT_EQ(connection_status,
            navigation->GetNavigationHandle()->GetSSLInfo()->connection_status);
}

namespace {

// Helper throttle which checks that it can access NavigationHandle's
// RenderFrameHost in WillFailRequest() and then defers the failure.
class GetRenderFrameHostOnFailureNavigationThrottle
    : public NavigationThrottle {
 public:
  explicit GetRenderFrameHostOnFailureNavigationThrottle(
      NavigationHandle* handle)
      : NavigationThrottle(handle) {}
  ~GetRenderFrameHostOnFailureNavigationThrottle() override = default;

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    EXPECT_TRUE(navigation_handle()->GetRenderFrameHost());
    return NavigationThrottle::DEFER;
  }

  const char* GetNameForLogging() override {
    return "GetRenderFrameHostOnFailureNavigationThrottle";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GetRenderFrameHostOnFailureNavigationThrottle);
};

class ThrottleTestContentBrowserClient : public ContentBrowserClient {
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle) override {
    std::vector<std::unique_ptr<NavigationThrottle>> throttle;
    throttle.push_back(
        std::make_unique<GetRenderFrameHostOnFailureNavigationThrottle>(
            navigation_handle));
    return throttle;
  }
};

}  // namespace

// Verify that the NavigationHandle::GetRenderFrameHost() can be retrieved by a
// throttle in WillFailRequest(), as well as after deferring the failure.  This
// is allowed, since at that point the final RenderFrameHost will have already
// been chosen. See https://crbug.com/817881.
TEST_F(NavigationRequestTest, WillFailRequestCanAccessRenderFrameHost) {
  std::unique_ptr<ContentBrowserClient> client(
      new ThrottleTestContentBrowserClient);
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(client.get());

  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->SetAutoAdvance(false);
  navigation->Start();
  navigation->Fail(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(
      NavigationRequest::WILL_FAIL_REQUEST,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_TRUE(navigation->GetNavigationHandle()->GetRenderFrameHost());
  NavigationRequest::From(navigation->GetNavigationHandle())
      ->GetNavigationThrottleRunnerForTesting()
      ->CallResumeForTesting();
  EXPECT_TRUE(navigation->GetNavigationHandle()->GetRenderFrameHost());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(NavigationRequestTest, PolicyContainerInheritance) {
  struct TestCase {
    const char* url;
    bool expect_inherit;
  } cases[]{{"about:blank", true},
            {"data:text/plain,hello", true},
            {"file://local", false},
            {"http://chromium.org", false}};

  const GURL kUrl1 = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl1, main_rfh());
  navigation->Commit();

  for (auto test : cases) {
    // We navigate child frames because the BlockedSchemeNavigationThrottle
    // restricts navigations in the main frame.
    auto* child_frame = static_cast<TestRenderFrameHost*>(
        content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));

    // We set the referrer policy of the frame to "always". We then create a new
    // navigation, set as initiator the frame itself, start the navigation, and
    // change the referrer policy of the frame to "never". After we commit the
    // navigation:
    // - If navigating to a local scheme, the target frame should have inherited
    //   the referrer policy of the initiator ("always").
    // - If navigating to a non-local scheme, the target frame should have a new
    //   policy container (hence referrer policy set to "default").
    const GURL kUrl = GURL(test.url);
    auto navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(kUrl, child_frame);
    static_cast<blink::mojom::PolicyContainerHost*>(
        child_frame->policy_container_host())
        ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kAlways);
    navigation->SetInitiatorFrame(child_frame);
    navigation->Start();
    static_cast<blink::mojom::PolicyContainerHost*>(
        child_frame->policy_container_host())
        ->SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
    navigation->Commit();
    EXPECT_EQ(
        test.expect_inherit ? network::mojom::ReferrerPolicy::kAlways
                            : network::mojom::ReferrerPolicy::kDefault,
        static_cast<RenderFrameHostImpl*>(navigation->GetFinalRenderFrameHost())
            ->policy_container_host()
            ->referrer_policy());
  }
}

TEST_F(NavigationRequestTest, DnsAliasesCanBeAccessed) {
  // Create simulated NavigationRequest for the URL, which has aliases.
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  std::vector<std::string> dns_aliases({"alias1", "alias2"});
  navigation->SetResponseDnsAliases(std::move(dns_aliases));

  // Start the navigation.
  navigation->Start();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Commit the navigation.
  navigation->set_http_connection_info(
      net::HttpResponseInfo::CONNECTION_INFO_QUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Verify that the aliases are accessible from the NavigationRequest.
  EXPECT_THAT(navigation->GetNavigationHandle()->GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2"));
}

TEST_F(NavigationRequestTest, NoDnsAliases) {
  // Create simulated NavigationRequest for the URL, which does not
  // have aliases. (Note the empty alias list.)
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  std::vector<std::string> dns_aliases;
  navigation->SetResponseDnsAliases(std::move(dns_aliases));

  // Start the navigation.
  navigation->Start();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Commit the navigation.
  navigation->set_http_connection_info(
      net::HttpResponseInfo::CONNECTION_INFO_QUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Verify that there are no aliases in the NavigationRequest.
  EXPECT_TRUE(navigation->GetNavigationHandle()->GetDnsAliases().empty());
}

// Test that the required CSP of every frame is computed/inherited correctly and
// that the Sec-Required-CSP header is set.
class CSPEmbeddedEnforcementUnitTest : public NavigationRequestTest {
 protected:
  TestRenderFrameHost* main_rfh() {
    return static_cast<TestRenderFrameHost*>(NavigationRequestTest::main_rfh());
  }

  // Simulate the |csp| attribute being set in |rfh|'s frame. Then navigate it.
  // Returns the request's Sec-Required-CSP header.
  std::string NavigateWithRequiredCSP(TestRenderFrameHost** rfh,
                                      std::string required_csp) {
    TestRenderFrameHost* document = *rfh;

    if (!required_csp.empty()) {
      auto headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
      headers->SetHeader("Content-Security-Policy", required_csp);
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies;
      network::AddContentSecurityPolicyFromHeaders(
          *headers, GURL("https://example.com/"), &policies);
      document->frame_tree_node()->set_csp_attribute(std::move(policies[0]));
    }

    // Chrome blocks a document navigating to a URL if more than one of its
    // ancestors have the same URL. Use a different URL every time, to
    // avoid blocking navigation of the grandchild frame.
    static int nonce = 0;
    GURL url("https://www.example.com" + base::NumberToString(nonce++));

    auto navigation =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation->Start();
    NavigationRequest* request =
        NavigationRequest::From(navigation->GetNavigationHandle());
    std::string sec_required_csp;
    request->GetRequestHeaders().GetHeader("sec-required-csp",
                                           &sec_required_csp);

    // Complete the navigation so that the required csp is stored in the
    // RenderFrameHost, so that when we will add children to this document they
    // will be able to get the parent's required csp (and hence also test that
    // the whole logic works).
    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    response_headers->SetHeader("Allow-CSP-From", "*");
    navigation->SetResponseHeaders(response_headers);
    navigation->Commit();

    *rfh = static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());

    return sec_required_csp;
  }

  TestRenderFrameHost* AddChild(TestRenderFrameHost* parent) {
    return static_cast<TestRenderFrameHost*>(
        content::RenderFrameHostTester::For(parent)->AppendChild(""));
  }
};

TEST_F(CSPEmbeddedEnforcementUnitTest, TopLevel) {
  TestRenderFrameHost* top_document = main_rfh();
  std::string sec_required_csp = NavigateWithRequiredCSP(&top_document, "");
  EXPECT_EQ("", sec_required_csp);
  EXPECT_FALSE(top_document->required_csp());
}

TEST_F(CSPEmbeddedEnforcementUnitTest, ChildNoCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  std::string sec_required_csp = NavigateWithRequiredCSP(&child_document, "");
  EXPECT_EQ("", sec_required_csp);
  EXPECT_FALSE(child_document->required_csp());
}

TEST_F(CSPEmbeddedEnforcementUnitTest, ChildWithCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest, ChildSiblingNoCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* sibling_document = AddChild(top_document);
  std::string sec_required_csp = NavigateWithRequiredCSP(&sibling_document, "");
  EXPECT_FALSE(sibling_document->required_csp());
}

TEST_F(CSPEmbeddedEnforcementUnitTest, ChildSiblingCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* sibling_document = AddChild(top_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&sibling_document, "script-src 'none'");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(sibling_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            sibling_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest, GrandChildNoCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&grand_child_document, "");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest, GrandChildSameCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&grand_child_document, "script-src 'none'");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest, GrandChildDifferentCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&grand_child_document, "img-src 'none'");

  // This seems weird, but it is the intended behaviour according to the spec.
  // The problem is that "script-src 'none'" does not subsume "img-src 'none'",
  // so "img-src 'none'" on the grandchild is an invalid csp attribute, and we
  // just discard it in favour of the parent's csp attribute.
  //
  // This should probably be fixed in the specification:
  // https://github.com/w3c/webappsec-cspee/pull/11
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest, InvalidCSP) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&child_document, "report-to group");
  EXPECT_EQ("", sec_required_csp);
  EXPECT_FALSE(child_document->required_csp());
}

TEST_F(CSPEmbeddedEnforcementUnitTest, InvalidCspAndInheritFromParent) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp =
      NavigateWithRequiredCSP(&grand_child_document, "report-to group");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest,
       SemiInvalidCspAndInheritSameCspFromParent) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp = NavigateWithRequiredCSP(
      &grand_child_document, "script-src 'none'; report-to group");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

TEST_F(CSPEmbeddedEnforcementUnitTest,
       SemiInvalidCspAndInheritDifferentCspFromParent) {
  TestRenderFrameHost* top_document = main_rfh();
  TestRenderFrameHost* child_document = AddChild(top_document);
  NavigateWithRequiredCSP(&child_document, "script-src 'none'");
  TestRenderFrameHost* grand_child_document = AddChild(child_document);
  std::string sec_required_csp = NavigateWithRequiredCSP(
      &grand_child_document, "sandbox; report-to group");
  EXPECT_EQ("script-src 'none'", sec_required_csp);
  EXPECT_TRUE(grand_child_document->required_csp());
  EXPECT_EQ("script-src 'none'",
            grand_child_document->required_csp()->header->header_value);
}

}  // namespace content
