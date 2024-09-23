// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/test/fenced_frame_test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

class NavigationRequestTest : public RenderViewHostImplTestHarness {
 public:
  NavigationRequestTest() : callback_result_(NavigationThrottle::DEFER) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    CreateNavigationHandle();
    contents()->GetPrimaryMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  void CancelDeferredNavigation(
      NavigationThrottle::ThrottleCheckResult result) {
    GetNavigationRequest()->CancelDeferredNavigationInternal(result);
  }

  // Helper function to call WillStartRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of
  // the throttle checks when they are finished.
  void SimulateWillStartRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    GetNavigationRequest()->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    GetNavigationRequest()->WillStartRequest();
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
    GetNavigationRequest()->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    GetNavigationRequest()->WillRedirectRequest(
        GURL(), nullptr /* post_redirect_process */);
  }

  // Helper function to call WillFailRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  void SimulateWillFailRequest(
      net::Error net_error_code,
      const std::optional<net::SSLInfo> ssl_info = std::nullopt) {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;
    GetNavigationRequest()->set_net_error(net_error_code);

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    GetNavigationRequest()->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    GetNavigationRequest()->WillFailRequest();
  }

  // Helper function to call WillCommitWithoutUrlLoader on |handle|. If this
  // function returns DEFER, |callback_result_| will be set to the actual result
  // of the throttle checks when they are finished.
  void SimulateWillCommitWithoutUrlLoader() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationRequest is owned by
    // the NavigationRequestTest.
    GetNavigationRequest()->set_complete_callback_for_testing(
        base::BindOnce(&NavigationRequestTest::UpdateThrottleCheckResult,
                       base::Unretained(this)));

    GetNavigationRequest()->WillCommitWithoutUrlLoader();
  }

  // Whether the callback was called.
  bool was_callback_called() const { return was_callback_called_; }

  // Returns the callback_result.
  NavigationThrottle::ThrottleCheckResult callback_result() const {
    return callback_result_;
  }

  NavigationRequest::NavigationState state() {
    return GetNavigationRequest()->state();
  }

  bool call_counts_match(TestNavigationThrottle* throttle,
                         int start,
                         int redirect,
                         int failure,
                         int process,
                         int withoutUrlLoader) {
    return start == throttle->GetCallCount(
                        TestNavigationThrottle::WILL_START_REQUEST) &&
           redirect == throttle->GetCallCount(
                           TestNavigationThrottle::WILL_REDIRECT_REQUEST) &&
           failure == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_FAIL_REQUEST) &&
           process == throttle->GetCallCount(
                          TestNavigationThrottle::WILL_PROCESS_RESPONSE) &&
           withoutUrlLoader ==
               throttle->GetCallCount(
                   TestNavigationThrottle::WILL_COMMIT_WITHOUT_URL_LOADER);
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on checks by default.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        new TestNavigationThrottle(GetNavigationRequest());
    test_throttle->SetResponseForAllMethods(TestNavigationThrottle::SYNCHRONOUS,
                                            result);
    GetNavigationRequest()->RegisterThrottleForTesting(
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
    auto common_params = blink::CreateCommonNavigationParams();
    common_params->initiator_origin =
        url::Origin::Create(GURL("https://initiator.example.com"));
    auto commit_params = blink::CreateCommitNavigationParams();
    commit_params->frame_policy =
        main_test_rfh()->frame_tree_node()->pending_frame_policy();
    auto request = NavigationRequest::CreateBrowserInitiated(
        main_test_rfh()->frame_tree_node(), std::move(common_params),
        std::move(commit_params), false /* was_opener_suppressed */,
        std::string() /* extra_headers */, nullptr /* frame_entry */,
        nullptr /* entry */, false /* is_form_submission */,
        nullptr /* navigation_ui_data */, std::nullopt /* impression */,
        false /* is_pdf */);
    main_test_rfh()->frame_tree_node()->TakeNavigationRequest(
        std::move(request));
    GetNavigationRequest()->StartNavigation();
  }

  FrameTreeNode* AddFrame(FrameTree& frame_tree,
                          RenderFrameHostImpl* parent,
                          int process_id,
                          int new_routing_id,
                          const blink::FramePolicy& frame_policy,
                          blink::FrameOwnerElementType owner_type) {
    return frame_tree.AddFrame(
        parent, process_id, new_routing_id,
        TestRenderFrameHost::CreateStubFrameRemote(),
        TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
        TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
        TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
        blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
        false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
        blink::DocumentToken(), frame_policy,
        blink::mojom::FrameOwnerProperties(), false, owner_type,
        /*is_dummy_frame_for_inner_tree=*/false);
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

  // This must be called after CreateNavigationHandle().
  NavigationRequest* GetNavigationRequest() {
    return main_test_rfh()->frame_tree_node()->navigation_request();
  }

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
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpConnectionInfo::kUNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(net::HttpConnectionInfo::kHTTP1_1);
  navigation->Redirect(kUrl2);
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpConnectionInfo::kHTTP1_1,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(net::HttpConnectionInfo::kQUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpConnectionInfo::kQUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());
}

TEST_F(NavigationRequestTest, SimpleDataCheckNoRedirect) {
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->Start();
  EXPECT_EQ(net::HttpConnectionInfo::kUNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->set_http_connection_info(net::HttpConnectionInfo::kQUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpConnectionInfo::kQUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());
}

TEST_F(NavigationRequestTest, SimpleDataChecksFailure) {
  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());
  navigation->Start();
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
            NavigationRequest::From(navigation->GetNavigationHandle())
                ->request_context_type());
  EXPECT_EQ(net::HttpConnectionInfo::kUNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  navigation->Fail(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(blink::mojom::RequestContextType::LOCATION,
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
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));
}

// Checks that a navigation deferred during WillRedirectRequest can be properly
// cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillRedirect) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_EQ(NavigationRequest::WILL_REDIRECT_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 1, 0, 0, 0));
}

// Checks that a navigation deferred during WillFailRequest can be properly
// cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillFail) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationRequest::WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0, 0));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0, 0));
}

// Checks that a navigation deferred can be canceled and not ignored.
TEST_F(NavigationRequestTest, CancelDeferredWillRedirectNoIgnore) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));
}

// Checks that a navigation deferred by WillFailRequest can be canceled and not
// ignored.
TEST_F(NavigationRequestTest, CancelDeferredWillFailNoIgnore) {
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(
      TestNavigationThrottle::WILL_FAIL_REQUEST, NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillStartRequest.
  SimulateWillStartRequest();
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 0, 0, 0));

  // Simulate WillFailRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillFailRequest(net::ERR_CERT_DATE_INVALID);
  EXPECT_EQ(NavigationRequest::WILL_FAIL_REQUEST, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0, 0));

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 1, 0, 1, 0, 0));
}

// Checks that a navigation deferred during WillCommitWithoutUrlLoader can be
// properly cancelled.
TEST_F(NavigationRequestTest, CancelDeferredWillCommitWithoutUrlLoader) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, state());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 0));

  // Simulate WillCommitWithoutUrlLoader. The request should be deferred. The
  // callback should not have been called.
  SimulateWillCommitWithoutUrlLoader();
  EXPECT_EQ(NavigationRequest::WILL_COMMIT_WITHOUT_URL_LOADER, state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 1));

  // Cancel the request. The callback should have been called.
  CancelDeferredNavigation(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_EQ(NavigationRequest::CANCELING, state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_TRUE(call_counts_match(test_throttle, 0, 0, 0, 0, 1));
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

TEST_F(NavigationRequestTest, SharedStorageWritable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kSharedStorageAPI,
                            blink::features::kSharedStorageAPIM118,
                            blink::features::kFencedFrames},
      /*disabled_features=*/{});

  // Create and start a simulated `NavigationRequest` for the main frame.
  GURL main_url = GURL("https://main.com");
  auto main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  main_navigation->Start();
  main_navigation->ReadyToCommit();

  // Verify that the main frame's `NavigationRequest` will not be
  // SharedStorageWritable.
  ASSERT_TRUE(main_navigation->GetNavigationHandle());
  EXPECT_FALSE(main_navigation->GetNavigationHandle()
                   ->shared_storage_writable_eligible());

  // Commit the navigation.
  main_navigation->Commit();

  // Append a child frame and set its `shared_storage_writable` attribute to
  // true.
  TestRenderFrameHost* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));
  blink::mojom::IframeAttributesPtr child_attributes =
      blink::mojom::IframeAttributes::New();
  child_attributes->shared_storage_writable_opted_in = true;
  child_frame->frame_tree_node()->SetAttributes(std::move(child_attributes));

  // Create and start a simulated `NavigationRequest` for the child frame.
  GURL a_url = GURL("https://a.com");
  auto child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(a_url, child_frame);
  child_navigation->Start();

  // Verify that the `NavigationRequest` will be SharedStorageWritable.
  ASSERT_TRUE(child_navigation->GetNavigationHandle());
  EXPECT_TRUE(child_navigation->GetNavigationHandle()
                  ->shared_storage_writable_eligible());

  // Commit the navigation.
  child_navigation->Commit();

  // Append a fenced frame and give it permission to access Shared Storage.
  TestRenderFrameHost* fenced_frame_root = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame());
  FrameTreeNode* fenced_frame_node =
      static_cast<RenderFrameHostImpl*>(fenced_frame_root)->frame_tree_node();
  FencedFrameConfig new_config = FencedFrameConfig(GURL("about:blank"));
  new_config.AddEffectiveEnabledPermissionForTesting(
      blink::mojom::PermissionsPolicyFeature::kSharedStorage);
  FencedFrameProperties new_props = FencedFrameProperties(new_config);
  fenced_frame_node->set_fenced_frame_properties(new_props);
  fenced_frame_root->ResetPermissionsPolicy({});

  // Append a child frame to the fenced frame root and set its
  // `shared_storage_writable` attribute to true.
  TestRenderFrameHost* child_of_fenced_frame =
      static_cast<TestRenderFrameHost*>(
          fenced_frame_root->AppendChild("child_of_fenced"));
  blink::mojom::IframeAttributesPtr child_of_fenced_frame_attributes =
      blink::mojom::IframeAttributes::New();
  child_of_fenced_frame_attributes->shared_storage_writable_opted_in = true;
  child_of_fenced_frame->frame_tree_node()->SetAttributes(
      std::move(child_of_fenced_frame_attributes));

  // Create and start a simulated `NavigationRequest` for the child frame.
  GURL b_url = GURL("https://b.com");
  auto child_of_fenced_frame_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url,
                                                       child_of_fenced_frame);
  child_of_fenced_frame_navigation->Start();

  // Verify that the `NavigationRequest` will be SharedStorageWritable.
  ASSERT_TRUE(child_of_fenced_frame_navigation->GetNavigationHandle());
  EXPECT_TRUE(child_of_fenced_frame_navigation->GetNavigationHandle()
                  ->shared_storage_writable_eligible());

  // Commit the navigation.
  child_of_fenced_frame_navigation->Commit();
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

  GetRenderFrameHostOnFailureNavigationThrottle(
      const GetRenderFrameHostOnFailureNavigationThrottle&) = delete;
  GetRenderFrameHostOnFailureNavigationThrottle& operator=(
      const GetRenderFrameHostOnFailureNavigationThrottle&) = delete;

  ~GetRenderFrameHostOnFailureNavigationThrottle() override = default;

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    EXPECT_TRUE(navigation_handle()->GetRenderFrameHost());
    return NavigationThrottle::DEFER;
  }

  const char* GetNameForLogging() override {
    return "GetRenderFrameHostOnFailureNavigationThrottle";
  }
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
    navigation =
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
  EXPECT_EQ(net::HttpConnectionInfo::kUNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Commit the navigation.
  navigation->set_http_connection_info(net::HttpConnectionInfo::kQUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpConnectionInfo::kQUIC_35,
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
  EXPECT_EQ(net::HttpConnectionInfo::kUNKNOWN,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Commit the navigation.
  navigation->set_http_connection_info(net::HttpConnectionInfo::kQUIC_35);
  navigation->ReadyToCommit();
  EXPECT_EQ(net::HttpConnectionInfo::kQUIC_35,
            navigation->GetNavigationHandle()->GetConnectionInfo());

  // Verify that there are no aliases in the NavigationRequest.
  EXPECT_TRUE(navigation->GetNavigationHandle()->GetDnsAliases().empty());
}

TEST_F(NavigationRequestTest, StorageKeyToCommit) {
  TestRenderFrameHost* child_document = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_rfh())->AppendChild(""));
  auto attributes = child_document->frame_tree_node()->attributes_->Clone();
  attributes->credentialless = true;
  child_document->frame_tree_node()->SetAttributes(std::move(attributes));

  const GURL kUrl = GURL("http://chromium.org");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, child_document);
  navigation->ReadyToCommit();
  NavigationRequest* request =
      NavigationRequest::From(navigation->GetNavigationHandle());
  EXPECT_TRUE(request->commit_params().storage_key.nonce().has_value());
  EXPECT_EQ(child_document->GetPage().credentialless_iframes_nonce(),
            request->commit_params().storage_key.nonce().value());

  navigation->Commit();
  child_document =
      static_cast<TestRenderFrameHost*>(navigation->GetFinalRenderFrameHost());
  EXPECT_TRUE(child_document->IsCredentialless());
  EXPECT_EQ(blink::StorageKey::CreateWithNonce(
                url::Origin::Create(kUrl),
                child_document->GetPage().credentialless_iframes_nonce()),
            child_document->GetStorageKey());
}

// Test that the StorageKey's value is correctly affected by the
// RuntimeFeatureStateContext.
TEST_F(NavigationRequestTest, RuntimeFeatureStateStorageKey) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Because the StorageKey's (and Storage Partitioning's) usage of
  // RuntimeFeatureState is only meant to disable partitioning (i.e.:
  // first-party only), we need the make sure the net::features is always
  // enabled.
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  // This lambda performs the navigation and compares the commit_params'
  // StorageKey against the passed in one. If `disable_sp` is true then it will
  // also enable the deprecation trial feature in the RFSC. It returns
  // the new TestRenderFrameHost* to the navigated frame.
  auto NavigateAndCompareKeys =
      [](NavigationSimulator* navigation, const blink::StorageKey& key,
         bool disable_sp = false) -> TestRenderFrameHost* {
    navigation->Start();

    NavigationRequest* request =
        NavigationRequest::From(navigation->GetNavigationHandle());

    if (disable_sp) {
      request->GetMutableRuntimeFeatureStateContext()
          .SetDisableThirdPartyStoragePartitioning2Enabled(true);
    }

    navigation->ReadyToCommit();

    EXPECT_EQ(key, request->commit_params().storage_key);

    navigation->Commit();
    return static_cast<TestRenderFrameHost*>(
        navigation->GetFinalRenderFrameHost());
  };

  // Throughout the test we'll be creating a frame tree with a main frame, a
  // child frame, and a grandchild frame.
  GURL main_url("https://main.com");
  GURL b_url("https://b.com");
  GURL c_url("https://c.com");

  url::Origin main_origin = url::Origin::Create(main_url);
  url::Origin b_origin = url::Origin::Create(b_url);
  url::Origin c_origin = url::Origin::Create(c_url);

  // Begin by testing with Storage Partitioning enabled.

  auto main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());

  // By definition the main frame's StorageKey will always be first party
  blink::StorageKey main_frame_key =
      blink::StorageKey::CreateFirstParty(main_origin);

  NavigateAndCompareKeys(main_navigation.get(), main_frame_key);

  TestRenderFrameHost* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));

  auto child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  // The child and grandchild should both be third-party keys.
  blink::StorageKey child_frame_key =
      blink::StorageKey::Create(b_origin, net::SchemefulSite(main_origin),
                                blink::mojom::AncestorChainBit::kCrossSite);

  child_frame = NavigateAndCompareKeys(child_navigation.get(), child_frame_key);

  TestRenderFrameHost* grandchild_frame =
      child_frame->AppendChild("grandchild");

  auto grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  blink::StorageKey grandchild_frame_key =
      blink::StorageKey::Create(c_origin, net::SchemefulSite(main_origin),
                                blink::mojom::AncestorChainBit::kCrossSite);
  grandchild_frame =
      NavigateAndCompareKeys(grandchild_navigation.get(), grandchild_frame_key);

  // Only the RuntimeFeatureStateContext in the main frame's matters. So
  // disabling Storage Partitioning in the child_frame shouldn't affect the
  // child's or the grandchild's StorageKey.
  child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  child_frame = NavigateAndCompareKeys(child_navigation.get(), child_frame_key,
                                       /*disable_sp=*/true);

  grandchild_frame = child_frame->AppendChild("grandchild");

  grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  grandchild_frame =
      NavigateAndCompareKeys(grandchild_navigation.get(), grandchild_frame_key);

  // Disabling Storage Partitioning on the main frame should cause the child's
  // and grandchild's StorageKey to be first-party.
  main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());

  NavigateAndCompareKeys(main_navigation.get(), main_frame_key,
                         /*disable_sp=*/true);

  child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child"));

  child_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(b_url, child_frame);

  // The child and grandchild should both be first-party keys.
  blink::StorageKey child_frame_key_1p =
      blink::StorageKey::CreateFirstParty(b_origin);

  child_frame =
      NavigateAndCompareKeys(child_navigation.get(), child_frame_key_1p);

  grandchild_frame = child_frame->AppendChild("grandchild");

  blink::StorageKey grandchild_frame_key_1p =
      blink::StorageKey::CreateFirstParty(c_origin);

  grandchild_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(c_url, grandchild_frame);

  grandchild_frame = NavigateAndCompareKeys(grandchild_navigation.get(),
                                            grandchild_frame_key_1p);
}

TEST_F(NavigationRequestTest,
       NavigationToCredentiallessDocumentNetworkIsolationInfo) {
  auto* child_frame = static_cast<TestRenderFrameHost*>(
      content::RenderFrameHostTester::For(main_test_rfh())
          ->AppendChild("child"));
  auto attributes = child_frame->frame_tree_node()->attributes_->Clone();
  attributes->credentialless = true;
  child_frame->frame_tree_node()->SetAttributes(std::move(attributes));

  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.com/navigation.html"), child_frame);
  navigation->ReadyToCommit();

  EXPECT_EQ(main_test_rfh()->GetPage().credentialless_iframes_nonce(),
            static_cast<NavigationRequest*>(navigation->GetNavigationHandle())
                ->isolation_info_for_subresources()
                .network_isolation_key()
                .GetNonce());
  EXPECT_EQ(main_test_rfh()->GetPage().credentialless_iframes_nonce(),
            static_cast<NavigationRequest*>(navigation->GetNavigationHandle())
                ->GetIsolationInfo()
                .network_isolation_key()
                .GetNonce());
}

TEST_F(NavigationRequestTest, UpdatePrivateNetworkRequestPolicy) {
  std::unique_ptr<NavigationSimulator> navigation =
      NavigationSimulator::CreateRendererInitiated(GURL("https://example.com/"),
                                                   main_test_rfh());
  navigation->SetSocketAddress(net::IPEndPoint());

  navigation->ReadyToCommit();
  NavigationRequest* request =
      NavigationRequest::From(navigation->GetNavigationHandle());
  EXPECT_FALSE(request->GetSocketAddress().address().IsValid());
  navigation->Commit();
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
      auto attributes = document->frame_tree_node()->attributes_->Clone();
      // Set csp value.
      attributes->parsed_csp_attribute = std::move(policies[0]);
      document->frame_tree_node()->SetAttributes(std::move(attributes));
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
    std::string sec_required_csp = request->GetRequestHeaders()
                                       .GetHeader("sec-required-csp")
                                       .value_or(std::string());

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

namespace {

// Mock that allows us to avoid depending on the origin_trials component.
class OriginTrialsControllerDelegateMock
    : public OriginTrialsControllerDelegate {
 public:
  ~OriginTrialsControllerDelegateMock() override = default;

  void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {
    persisted_tokens_[origin] =
        std::vector<std::string>(header_tokens.begin(), header_tokens.end());
  }
  void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const url::Origin> script_origins,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override {
    NOTREACHED_IN_MIGRATION() << "not used by test";
  }
  bool IsFeaturePersistedForOrigin(const url::Origin& origin,
                                   const url::Origin& partition_origin,
                                   blink::mojom::OriginTrialFeature feature,
                                   const base::Time current_time) override {
    DCHECK(false) << "Method not implemented for test.";
    return false;
  }

  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::Time current_time) override {
    DCHECK(false) << "Method not implemented for test.";
    return base::flat_set<std::string>();
  }

  void ClearPersistedTokens() override { persisted_tokens_.clear(); }

  base::flat_map<url::Origin, std::vector<std::string>> persisted_tokens_;
};

}  // namespace

class PersistentOriginTrialNavigationRequestTest
    : public NavigationRequestTest {
 public:
  PersistentOriginTrialNavigationRequestTest()
      : delegate_mock_(std::make_unique<OriginTrialsControllerDelegateMock>()) {

  }
  ~PersistentOriginTrialNavigationRequestTest() override = default;

  std::vector<std::string> GetPersistedTokens(const url::Origin& origin) {
    return delegate_mock_->persisted_tokens_[origin];
  }

 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    std::unique_ptr<TestBrowserContext> context =
        std::make_unique<TestBrowserContext>();
    context->SetOriginTrialsControllerDelegate(delegate_mock_.get());
    return context;
  }

 private:
  std::unique_ptr<OriginTrialsControllerDelegateMock> delegate_mock_;
};

// Ensure that navigations with a valid Origin-Trial header with a persistent
// origin trial token results in the trial being marked as enabled.
// Then check that subsequent navigations without headers trigger an update
// that clears out stored trials.
TEST_F(PersistentOriginTrialNavigationRequestTest,
       NavigationCommitsPersistentOriginTrials) {
  // Generated with:
  // tools/origin_trials/generate_token.py https://example.com
  // FrobulatePersistent
  // --expire-timestamp=2000000000
  const char kPersistentOriginTrialToken[] =
      "AzZfd1vKZ0SSGRGk/"
      "8nIszQSlHYjbuYVE3jwaNZG3X4t11zRhzPWWJwTZ+JJDS3JJsyEZcpz+y20pAP6/"
      "6upOQ4AAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZ"
      "SI"
      "6ICJGcm9idWxhdGVQZXJzaXN0ZW50IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPersistentOriginTrials);
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;

  const GURL kUrl = GURL("https://example.com");
  auto navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kPersistentOriginTrialToken);
  navigation->SetResponseHeaders(response_headers);

  navigation->Commit();

  url::Origin origin = url::Origin::Create(kUrl);
  EXPECT_EQ(std::vector<std::string>{kPersistentOriginTrialToken},
            GetPersistedTokens(origin));

  // Navigate again without response headers to assert the trial information is
  // still updated and cleared.
  NavigationSimulatorImpl::CreateRendererInitiated(kUrl, main_rfh())->Commit();
  EXPECT_EQ(std::vector<std::string>(), GetPersistedTokens(origin));
}

namespace {

// Test version of a NavigationThrottle that requests the response body.
class ResponseBodyNavigationThrottle : public NavigationThrottle {
 public:
  using ResponseBodyCallback = base::OnceCallback<void(const std::string&)>;

  ResponseBodyNavigationThrottle(NavigationHandle* handle,
                                 ResponseBodyCallback callback)
      : NavigationThrottle(handle), callback_(std::move(callback)) {}
  ResponseBodyNavigationThrottle(const ResponseBodyNavigationThrottle&) =
      delete;
  ResponseBodyNavigationThrottle& operator=(
      const ResponseBodyNavigationThrottle&) = delete;
  ~ResponseBodyNavigationThrottle() override = default;

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    navigation_handle()->GetResponseBody(
        base::BindOnce(&ResponseBodyNavigationThrottle::OnResponseBodyReady,
                       base::Unretained(this)));
    return NavigationThrottle::DEFER;
  }

  const char* GetNameForLogging() override {
    return "ResponseBodyNavigationThrottle";
  }

 private:
  void OnResponseBodyReady(const std::string& response_body) {
    std::move(callback_).Run(response_body);
    NavigationRequest::From(navigation_handle())
        ->GetNavigationThrottleRunnerForTesting()
        ->CallResumeForTesting();
  }

  ResponseBodyCallback callback_;
};

}  // namespace

// Tests response body.
class NavigationRequestResponseBodyTest : public NavigationRequestTest {
 public:
  std::unique_ptr<NavigationSimulator> CreateNavigationSimulator() {
    auto navigation = NavigationSimulatorImpl::CreateRendererInitiated(
        GURL("http://example.test"), main_rfh());
    navigation->SetAutoAdvance(false);
    navigation->Start();
    // It is safe to use base::Unretained as the NavigationThrottle will not be
    // destroyed before the callback is called.
    auto throttle = std::make_unique<ResponseBodyNavigationThrottle>(
        navigation->GetNavigationHandle(),
        base::BindOnce(&NavigationRequestResponseBodyTest::UpdateResponseBody,
                       base::Unretained(this)));
    navigation->GetNavigationHandle()->RegisterThrottleForTesting(
        std::move(throttle));
    return navigation;
  }

  void UpdateResponseBody(const std::string& response_body) {
    response_body_ = response_body;
    was_callback_called_ = true;
  }

  bool was_callback_called() const { return was_callback_called_; }

  const std::string& response_body() const { return response_body_; }

 protected:
  mojo::ScopedDataPipeProducerHandle producer_handle_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

 private:
  bool was_callback_called_ = false;
  std::string response_body_;
};

TEST_F(NavigationRequestResponseBodyTest, Received) {
  auto navigation = CreateNavigationSimulator();
  std::string response = "response-body-content";
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(response.size(), producer_handle_,
                                 consumer_handle_));
  navigation->SetResponseBody(std::move(consumer_handle_));

  navigation->ReadyToCommit();
  EXPECT_EQ(
      NavigationRequest::WILL_PROCESS_RESPONSE,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(std::string(), response_body());

  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle_->WriteData(base::as_byte_span(response),
                                        MOJO_WRITE_DATA_FLAG_NONE,
                                        actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, response.size());

  navigation->Wait();
  EXPECT_EQ(
      NavigationRequest::READY_TO_COMMIT,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(response, response_body());
}

TEST_F(NavigationRequestResponseBodyTest, PartiallyReceived) {
  auto navigation = CreateNavigationSimulator();

  // The data pipe size is smaller than the response body size.
  uint32_t pipe_size = 8u;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(pipe_size, producer_handle_,
                                                 consumer_handle_));
  navigation->SetResponseBody(std::move(consumer_handle_));

  navigation->ReadyToCommit();
  EXPECT_EQ(
      NavigationRequest::WILL_PROCESS_RESPONSE,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(std::string(), response_body());

  std::string response = "response-body-content";
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle_->WriteData(base::as_byte_span(response),
                                        MOJO_WRITE_DATA_FLAG_NONE,
                                        actually_written_bytes));
  EXPECT_EQ(actually_written_bytes, pipe_size);

  navigation->Wait();
  EXPECT_EQ(
      NavigationRequest::READY_TO_COMMIT,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_TRUE(was_callback_called());
  // Only the first part of the response body that fits in the pipe is received.
  EXPECT_EQ("response", response_body());
}

TEST_F(NavigationRequestResponseBodyTest, PipeClosed) {
  auto navigation = CreateNavigationSimulator();
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(10u, producer_handle_, consumer_handle_));
  navigation->SetResponseBody(std::move(consumer_handle_));
  navigation->ReadyToCommit();
  EXPECT_EQ(
      NavigationRequest::WILL_PROCESS_RESPONSE,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(std::string(), response_body());

  // Close the pipe before any data is sent.
  producer_handle_.reset();
  navigation->Wait();
  EXPECT_EQ(
      NavigationRequest::READY_TO_COMMIT,
      NavigationRequest::From(navigation->GetNavigationHandle())->state());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(std::string(), response_body());
}

TEST_F(NavigationRequestTest, ViewTransitionForceEnablesPageSwap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({blink::features::kViewTransitionOnNavigation},
                                {});

  GURL main_url = GURL("https://main.com");
  auto main_navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(main_url, contents());
  main_navigation->Start();
  ASSERT_TRUE(
      main_navigation->GetNavigationHandle()->ShouldDispatchPageSwapEvent());
}

}  // namespace content
