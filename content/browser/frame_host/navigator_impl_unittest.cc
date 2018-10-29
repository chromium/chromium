// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

// TODO(clamy): Rename the tests NavigatorTests.
class NavigatorTestWithBrowserSideNavigation
    : public RenderViewHostImplTestHarness {
 public:
  using SiteInstanceDescriptor = RenderFrameHostManager::SiteInstanceDescriptor;
  using SiteInstanceRelation = RenderFrameHostManager::SiteInstanceRelation;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
  }

  void TearDown() override {
    RenderViewHostImplTestHarness::TearDown();
  }

  TestNavigationURLLoader* GetLoaderForNavigationRequest(
      NavigationRequest* request) const {
    return static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  }

  // Requests a navigation of the specified FrameTreeNode to the specified URL;
  // returns the unique ID of the pending NavigationEntry.
  int RequestNavigation(FrameTreeNode* node, const GURL& url) {
    return RequestNavigationWithParameters(node, url, Referrer(),
                                           ui::PAGE_TRANSITION_LINK);
  }

  // Requests a navigation of the specified FrameTreeNode to the specified URL,
  // using other specified parameters; returns the unique ID of the pending
  // NavigationEntry.
  int RequestNavigationWithParameters(
      FrameTreeNode* node,
      const GURL& url,
      const Referrer& referrer,
      ui::PageTransition transition_type) {
    NavigationController::LoadURLParams load_params(url);
    load_params.frame_tree_node_id = node->frame_tree_node_id();
    load_params.referrer = referrer;
    load_params.transition_type = transition_type;

    controller().LoadURLWithParams(load_params);
    return controller().GetPendingEntry()->GetUniqueID();
  }

  TestRenderFrameHost* GetSpeculativeRenderFrameHost(FrameTreeNode* node) {
    return static_cast<TestRenderFrameHost*>(
        node->render_manager()->speculative_render_frame_host_.get());
  }

  scoped_refptr<SiteInstance> ConvertToSiteInstance(
      RenderFrameHostManager* rfhm,
      const SiteInstanceDescriptor& descriptor,
      SiteInstance* candidate_instance) {
    return rfhm->ConvertToSiteInstance(descriptor, candidate_instance);
  }
};

// PlzNavigate: Test a complete browser-initiated navigation starting with a
// non-live renderer.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SimpleBrowserInitiatedNavigationFromNonLiveRenderer) {
  const GURL kUrl("http://chromium.org/");

  EXPECT_FALSE(main_test_rfh()->IsRenderFrameLive());

  // Start a browser-initiated navigation.
  int32_t site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  int entry_id = RequestNavigation(node, kUrl);
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(kUrl, request->common_params().url);
  EXPECT_TRUE(request->browser_initiated());

  // As there's no live renderer the navigation should not wait for a
  // beforeUnload ACK from the renderer and start right away.
  EXPECT_EQ(NavigationRequest::STARTED, request->state());
  ASSERT_TRUE(GetLoaderForNavigationRequest(request));
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  int64_t navigation_id = request->navigation_handle()->GetNavigationId();

  // Have the current RenderFrameHost commit the navigation.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(request)->CallOnResponseStarted(response,
                                                                nullptr);
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  main_test_rfh()->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(main_test_rfh()->navigation_request());
  main_test_rfh()->SendNavigate(entry_id, true, kUrl);
  EXPECT_TRUE(main_test_rfh()->is_active());
  EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
            main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl, contents()->GetLastCommittedURL());

  // The main RenderFrameHost should not have been changed, and the renderer
  // should have been initialized.
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // After a navigation is finished no speculative RenderFrameHost should
  // exist.
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test a complete renderer-initiated same-site navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SimpleRendererInitiatedSameSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidStopLoading(main_test_rfh()->GetRoutingID()));

  // Start a renderer-initiated non-user-initiated navigation.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  navigation->SetHasUserGesture(false);
  navigation->Start();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);

  // The navigation is immediately started as there's no need to wait for
  // beforeUnload to be executed.
  EXPECT_EQ(NavigationRequest::STARTED, request->state());
  EXPECT_FALSE(request->common_params().has_user_gesture);
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_FALSE(main_test_rfh()->is_loading());

  // Have the current RenderFrameHost commit the navigation
  navigation->ReadyToCommit();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->is_active());
  EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl2),
            main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Test a complete renderer-initiated navigation that should be
// cross-site but does not result in a SiteInstance swap because its
// renderer-initiated.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SimpleRendererInitiatedCrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com");

  contents()->NavigateAndCommit(kUrl1);
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidStopLoading(main_test_rfh()->GetRoutingID()));
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  int32_t site_instance_id_1 = main_test_rfh()->GetSiteInstance()->GetId();

  // Start a renderer-initiated navigation.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->Start();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);

  // The navigation is immediately started as there's no need to wait for
  // beforeUnload to be executed.
  EXPECT_EQ(NavigationRequest::STARTED, request->state());
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());
  EXPECT_FALSE(main_test_rfh()->is_loading());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Have the current RenderFrameHost commit the navigation.
  navigation->ReadyToCommit();
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(navigation->GetFinalRenderFrameHost(),
              GetSpeculativeRenderFrameHost(node));
  }
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_TRUE(main_test_rfh()->is_active());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // The SiteInstance did not change unless site-per-process is enabled.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(site_instance_id_1, main_test_rfh()->GetSiteInstance()->GetId());
  } else {
    EXPECT_EQ(site_instance_id_1, main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// PlzNavigate: Test that a beforeUnload denial cancels the navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       BeforeUnloadDenialCancelNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  contents()->NavigateAndCommit(kUrl1);

  // Start a new navigation.
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  RequestNavigation(node, kUrl2);
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->browser_initiated());
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE, request->state());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  RenderFrameDeletedObserver rfh_deleted_observer(
      GetSpeculativeRenderFrameHost(node));

  // Simulate a beforeUnload denial.
  main_test_rfh()->SendBeforeUnloadACK(false);
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_TRUE(rfh_deleted_observer.deleted());
}

// PlzNavigate: Test that a proper NavigationRequest is created by
// RequestNavigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, BeginNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");
  const GURL kUrl3("http://www.gmail.com/");

  contents()->NavigateAndCommit(kUrl1);

  // Add a subframe.
  FrameTreeNode* root_node = contents()->GetFrameTree()->root();
  TestRenderFrameHost* subframe_rfh = main_test_rfh()->AppendChild("Child");
  ASSERT_TRUE(subframe_rfh);

  // Start a navigation at the subframe.
  FrameTreeNode* subframe_node = subframe_rfh->frame_tree_node();
  RequestNavigation(subframe_node, kUrl2);
  NavigationRequest* subframe_request = subframe_node->navigation_request();

  // We should be waiting for the BeforeUnload event to execute in the subframe.
  ASSERT_TRUE(subframe_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            subframe_request->state());
  EXPECT_TRUE(subframe_rfh->is_waiting_for_beforeunload_ack());

  // Simulate the BeforeUnload ACK. The navigation should start.
  subframe_rfh->SendBeforeUnloadACK(true);
  TestNavigationURLLoader* subframe_loader =
      GetLoaderForNavigationRequest(subframe_request);
  ASSERT_TRUE(subframe_loader);
  EXPECT_EQ(NavigationRequest::STARTED, subframe_request->state());
  EXPECT_EQ(kUrl2, subframe_request->common_params().url);
  EXPECT_EQ(kUrl2, subframe_loader->request_info()->common_params.url);
  // First party for cookies url should be that of the main frame.
  EXPECT_EQ(kUrl1, subframe_loader->request_info()->site_for_cookies);
  EXPECT_FALSE(subframe_loader->request_info()->is_main_frame);
  EXPECT_TRUE(subframe_loader->request_info()->parent_is_main_frame);
  EXPECT_TRUE(subframe_request->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(root_node));

  // Subframe navigations should never create a speculative RenderFrameHost,
  // unless site-per-process is enabled. In that case, as the subframe
  // navigation is to a different site and is still ongoing, it should have one.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(subframe_node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(subframe_node));
  }

  // Now start a navigation at the root node.
  RequestNavigation(root_node, kUrl3);
  NavigationRequest* main_request = root_node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            main_request->state());

  // Main frame navigation to a different site should use a speculative
  // RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // Simulate a BeforeUnloadACK IPC on the main frame.
  main_test_rfh()->SendBeforeUnloadACK(true);
  TestNavigationURLLoader* main_loader =
      GetLoaderForNavigationRequest(main_request);
  EXPECT_EQ(kUrl3, main_request->common_params().url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->common_params.url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->site_for_cookies);
  EXPECT_TRUE(main_loader->request_info()->is_main_frame);
  EXPECT_FALSE(main_loader->request_info()->parent_is_main_frame);
  EXPECT_TRUE(main_request->browser_initiated());
  // BeforeUnloadACK was received from the renderer so the navigation should
  // have started.
  EXPECT_EQ(NavigationRequest::STARTED, main_request->state());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // As the main frame hasn't yet committed the subframe still exists. Thus, the
  // above situation regarding subframe navigations is valid here.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(subframe_node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(subframe_node));
  }
}

// PlzNavigate: Test that committing an HTTP 204 or HTTP 205 response cancels
// the navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, NoContent) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Load a URL.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);

  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);

  // Navigations to a different site do create a speculative RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 204 response.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  const char kNoContentHeaders[] = "HTTP/1.1 204 No Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kNoContentHeaders, arraysize(kNoContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, nullptr);

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now, repeat the test with 205 Reset Content.

  // Navigate to a different site again.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);

  main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 205 response.
  response = new network::ResourceResponse;
  const char kResetContentHeaders[] = "HTTP/1.1 205 Reset Content\0\0";
  response->head.headers = new net::HttpResponseHeaders(
      std::string(kResetContentHeaders, arraysize(kResetContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, nullptr);

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Test that a new RenderFrameHost is created when doing a cross site
// navigation.
TEST_F(NavigatorTestWithBrowserSideNavigation, CrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* initial_rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  int entry_id = RequestNavigation(node, kUrl2);
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  int64_t navigation_id = main_request->navigation_handle()->GetNavigationId();
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));

  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, nullptr);
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_FALSE(main_test_rfh()->navigation_request());

  speculative_rfh->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(speculative_rfh->navigation_request());

  speculative_rfh->SendNavigate(entry_id, true, kUrl2);

  RenderFrameHostImpl* final_rfh = main_test_rfh();
  EXPECT_EQ(speculative_rfh, final_rfh);
  EXPECT_NE(initial_rfh, final_rfh);
  EXPECT_TRUE(final_rfh->IsRenderFrameLive());
  EXPECT_TRUE(final_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Test that redirects are followed and the speculative RenderFrameHost logic
// behaves as expected.
TEST_F(NavigatorTestWithBrowserSideNavigation, RedirectCrossSite) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a URL on the same site.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  int entry_id = RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // It then redirects to another site.
  GetLoaderForNavigationRequest(main_request)->SimulateServerRedirect(kUrl2);

  // The redirect should have been followed.
  EXPECT_EQ(1, GetLoaderForNavigationRequest(main_request)->redirect_count());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  int64_t navigation_id = main_request->navigation_handle()->GetNavigationId();

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, nullptr);
  TestRenderFrameHost* final_speculative_rfh =
      GetSpeculativeRenderFrameHost(node);
  EXPECT_TRUE(final_speculative_rfh);

  // Commit the navigation.
  final_speculative_rfh->SimulateCommitProcessed(navigation_id,
                                                 true /* was_successful */);
  EXPECT_TRUE(final_speculative_rfh->navigation_request());
  final_speculative_rfh->SendNavigate(entry_id, true, kUrl2);
  RenderFrameHostImpl* final_rfh = main_test_rfh();
  ASSERT_TRUE(final_rfh);
  EXPECT_NE(rfh, final_rfh);
  EXPECT_EQ(final_speculative_rfh, final_rfh);
  EXPECT_TRUE(final_rfh->IsRenderFrameLive());
  EXPECT_TRUE(final_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Test that a navigation is canceled if another browser-initiated request has
// been issued in the meantime. Also confirms that the speculative
// RenderFrameHost is correctly updated in the process.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       BrowserInitiatedNavigationCancel) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl1_site = SiteInstance::GetSiteForURL(browser_context(), kUrl1);
  const GURL kUrl2("http://www.google.com/");
  const GURL kUrl2_site = SiteInstance::GetSiteForURL(browser_context(), kUrl2);

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Request navigation to the 1st URL.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Confirm a speculative RenderFrameHost was created.
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32_t site_instance_id_1 = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_EQ(kUrl1_site, speculative_rfh->GetSiteInstance()->GetSiteURL());

  // Request navigation to the 2nd URL; the NavigationRequest must have been
  // replaced by a new one with a different URL.
  int entry_id = RequestNavigation(node, kUrl2);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request2 = node->navigation_request();
  int64_t navigation_id = request2->navigation_handle()->GetNavigationId();
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that a new speculative RenderFrameHost was created.
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32_t site_instance_id_2 = speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(site_instance_id_1, site_instance_id_2);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(request2)->CallOnResponseStarted(response,
                                                                 nullptr);
  // Commit the navigation.
  speculative_rfh->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(speculative_rfh->navigation_request());
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  speculative_rfh->SendNavigate(entry_id, true, kUrl2);

  // Confirm that the commit corresponds to the new request.
  ASSERT_TRUE(main_test_rfh());
  EXPECT_EQ(kUrl2_site, main_test_rfh()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  // Confirm that the committed RenderFrameHost is the latest speculative one.
  EXPECT_EQ(site_instance_id_2, main_test_rfh()->GetSiteInstance()->GetId());
}

// Test that a browser-initiated navigation is canceled if a renderer-initiated
// user-initiated request has been issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererUserInitiatedNavigationCancel) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL and receive its
  // beforeUnload ACK.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  RequestNavigation(node, kUrl1);
  main_test_rfh()->SendBeforeUnloadACK(true);
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Confirm that a speculative RenderFrameHost was created.
  ASSERT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Now receive a renderer-initiated user-initiated request. It should replace
  // the current NavigationRequest.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation->SetHasUserGesture(true);
  navigation->Start();
  NavigationRequest* request2 = node->navigation_request();
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_TRUE(request2->common_params().has_user_gesture);

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that the speculative RenderFrameHost was destroyed in the non
  // SitePerProcess case.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Commit the navigation.
  navigation->Commit();

  // Confirm that the commit corresponds to the new request.
  ASSERT_TRUE(main_test_rfh());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a renderer-initiated user-initiated navigation is
// canceled if a renderer-initiated non-user-initiated request is issued in the
// meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationCancelsRendererUserInitiated) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a renderer-initiated user-initiated navigation to the 1st URL.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto user_initiated_navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  user_initiated_navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  user_initiated_navigation->SetHasUserGesture(true);
  user_initiated_navigation->Start();
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_FALSE(request1->browser_initiated());
  EXPECT_TRUE(request1->common_params().has_user_gesture);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Now receive a renderer-initiated non-user-initiated request. The previous
  // navigation should be replaced.
  auto non_user_initiated_navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  non_user_initiated_navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
  non_user_initiated_navigation->SetHasUserGesture(false);
  non_user_initiated_navigation->Start();

  NavigationRequest* request2 = node->navigation_request();
  ASSERT_TRUE(request2);
  EXPECT_NE(request1, request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_FALSE(request2->common_params().has_user_gesture);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Commit the navigation.
  non_user_initiated_navigation->Commit();
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a browser-initiated navigation is NOT canceled if a
// renderer-initiated non-user-initiated request is issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationDoesntCancelBrowserInitiated) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  int entry_id = RequestNavigation(node, kUrl1);
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Now receive a renderer-initiated non-user-initiated request. Nothing should
  // change.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(
      kUrl2, false /* has_user_gesture */);
  NavigationRequest* request2 = node->navigation_request();
  ASSERT_TRUE(request2);
  EXPECT_EQ(request1, request2);
  EXPECT_EQ(kUrl1, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Now receive the beforeUnload ACK from the still ongoing navigation.
  main_test_rfh()->SendBeforeUnloadACK(true);
  int64_t navigation_id = request1->navigation_handle()->GetNavigationId();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Have the RenderFrameHost commit the navigation.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(request2)->CallOnResponseStarted(response,
                                                                 nullptr);
  speculative_rfh->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(speculative_rfh->navigation_request());
  EXPECT_FALSE(main_test_rfh()->navigation_request());

  // Commit the navigation.
  speculative_rfh->SendNavigate(entry_id, true, kUrl1);
  EXPECT_EQ(kUrl1, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a renderer-initiated non-user-initiated navigation is
// canceled if a another similar request is issued in the meantime.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       RendererNonUserInitiatedNavigationCancelSimilarNavigation) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  int32_t site_instance_id_0 = main_test_rfh()->GetSiteInstance()->GetId();

  // Start a renderer-initiated non-user-initiated navigation to the 1st URL.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation1 =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  navigation1->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  navigation1->SetHasUserGesture(false);
  navigation1->Start();
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_FALSE(request1->browser_initiated());
  EXPECT_FALSE(request1->common_params().has_user_gesture);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }
  base::WeakPtr<TestNavigationURLLoader> loader1 =
      GetLoaderForNavigationRequest(request1)->AsWeakPtr();
  EXPECT_TRUE(loader1);

  // Now receive a 2nd similar request that should replace the current one.
  auto navigation2 =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation2->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  navigation2->SetHasUserGesture(false);
  navigation2->Start();
  NavigationRequest* request2 = node->navigation_request();
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_FALSE(request2->browser_initiated());
  EXPECT_FALSE(request2->common_params().has_user_gesture);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Commit the navigation.
  navigation2->Commit();
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  // The SiteInstance did not change unless site-per-process is enabled.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  } else {
    EXPECT_EQ(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// PlzNavigate: Test that a reload navigation is properly signaled to the
// RenderFrame when the navigation can commit. A speculative RenderFrameHost
// should not be created at any step.
TEST_F(NavigatorTestWithBrowserSideNavigation, Reload) {
  const GURL kUrl("http://www.google.com/");
  contents()->NavigateAndCommit(kUrl);

  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  controller().Reload(ReloadType::NORMAL, false);
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  // A NavigationRequest should have been generated.
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD,
            main_request->common_params().navigation_type);
  main_test_rfh()->PrepareForCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  main_test_rfh()->SendNavigate(entry_id, false, kUrl);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now do a shift+reload.
  controller().Reload(ReloadType::BYPASSING_CACHE, false);
  // A NavigationRequest should have been generated.
  main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE,
            main_request->common_params().navigation_type);
  main_test_rfh()->PrepareForCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is used when
// navigating from one site to another.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SpeculativeRendererWorksBaseCase) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  int entry_id = RequestNavigation(node, kUrl);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_NE(speculative_rfh, main_test_rfh());

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
            speculative_rfh->GetSiteInstance()->GetSiteURL());
  int32_t site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  int64_t navigation_id =
      node->navigation_request()->navigation_handle()->GetNavigationId();

  // Ask Navigator to commit the navigation by simulating a call to
  // OnResponseStarted.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(node->navigation_request())
      ->CallOnResponseStarted(response, nullptr);
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());

  // Invoke DidCommitProvisionalLoad.
  speculative_rfh->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(speculative_rfh->navigation_request());
  speculative_rfh->SendNavigate(entry_id, true, kUrl);
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is thrown away when
// the final URL's site differs from the initial one due to redirects.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SpeculativeRendererDiscardedAfterRedirectToAnotherSite) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  int32_t init_site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  int entry_id = RequestNavigation(node, kUrl);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32_t site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  RenderFrameDeletedObserver rfh_deleted_observer(speculative_rfh);
  EXPECT_NE(init_site_instance_id, site_instance_id);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_NE(speculative_rfh, main_test_rfh());
  EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
            speculative_rfh->GetSiteInstance()->GetSiteURL());

  // Receive the beforeUnload ACK.
  main_test_rfh()->SendBeforeUnloadACK(true);
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));

  // It then redirects to yet another site.
  NavigationRequest* main_request = node->navigation_request();
  int64_t navigation_id = main_request->navigation_handle()->GetNavigationId();
  ASSERT_TRUE(main_request);
  const GURL kUrlRedirect("https://www.google.com/");
  GetLoaderForNavigationRequest(main_request)
      ->SimulateServerRedirect(kUrlRedirect);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());

  // For now, ensure that the speculative RenderFrameHost does not change after
  // the redirect.
  // TODO(carlosk): once the speculative RenderFrameHost updates with redirects
  // this next check will be changed to verify that it actually happens.
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());
  EXPECT_FALSE(rfh_deleted_observer.deleted());

  // Commit the navigation with Navigator by simulating the call to
  // OnResponseStarted.
  scoped_refptr<network::ResourceResponse> response(
      new network::ResourceResponse);
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(response, nullptr);
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_TRUE(rfh_deleted_observer.deleted());

  // Once commit happens the speculative RenderFrameHost is updated to match the
  // known final SiteInstance.
  EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlRedirect),
            speculative_rfh->GetSiteInstance()->GetSiteURL());
  int32_t redirect_site_instance_id =
      speculative_rfh->GetSiteInstance()->GetId();
  EXPECT_NE(init_site_instance_id, redirect_site_instance_id);
  EXPECT_NE(site_instance_id, redirect_site_instance_id);

  // Invoke DidCommitProvisionalLoad.
  speculative_rfh->SimulateCommitProcessed(navigation_id,
                                           true /* was_successful */);
  EXPECT_TRUE(speculative_rfh->navigation_request());
  speculative_rfh->SendNavigate(entry_id, true, kUrlRedirect);

  // Check that the speculative RenderFrameHost was swapped in.
  EXPECT_EQ(redirect_site_instance_id,
            main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Verify that data urls are properly handled.
TEST_F(NavigatorTestWithBrowserSideNavigation, DataUrls) {
  const GURL kUrl1("http://wikipedia.org/");
  const GURL kUrl2("data:text/html,test");

  // Navigate to an initial site.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a data url. The request should have been sent to the IO
  // thread and not committed immediately.
  int entry_id = RequestNavigation(node, kUrl2);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_FALSE(speculative_rfh->is_loading());
  EXPECT_TRUE(node->navigation_request());
  speculative_rfh->PrepareForCommit();
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_NE(main_test_rfh(), speculative_rfh);
  speculative_rfh->SendNavigate(entry_id, true, kUrl2);
  EXPECT_EQ(main_test_rfh(), speculative_rfh);

  // Go back to the initial site.
  contents()->NavigateAndCommit(kUrl1);

  // Do a renderer-initiated navigation to a data url. The request should be
  // sent to the IO thread.
  auto navigation_to_data_url =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation_to_data_url->Start();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_TRUE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Tests several cases for converting SiteInstanceDescriptors into
// SiteInstances:
// 1) Pointer to the current SiteInstance.
// 2) Pointer to an unrelated SiteInstance.
// 3) Same-site URL, related.
// 4) Cross-site URL, related.
// 5) Same-site URL, unrelated (with and without candidate SiteInstances).
// 6) Cross-site URL, unrelated (with candidate SiteInstance).
TEST_F(NavigatorTestWithBrowserSideNavigation,
       SiteInstanceDescriptionConversion) {
  // Navigate to set a current SiteInstance on the RenderFrameHost.
  GURL kUrl1("http://a.com");
  contents()->NavigateAndCommit(kUrl1);
  SiteInstance* current_instance = main_test_rfh()->GetSiteInstance();
  ASSERT_TRUE(current_instance);

  // 1) Convert a descriptor pointing to the current instance.
  RenderFrameHostManager* rfhm =
      main_test_rfh()->frame_tree_node()->render_manager();
  {
    SiteInstanceDescriptor descriptor(current_instance);
    scoped_refptr<SiteInstance> converted_instance =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    EXPECT_EQ(current_instance, converted_instance);
  }

  // 2) Convert a descriptor pointing an instance unrelated to the current one,
  // with a different site.
  GURL kUrl2("http://b.com");
  scoped_refptr<SiteInstance> unrelated_instance(
      SiteInstance::CreateForURL(browser_context(), kUrl2));
  EXPECT_FALSE(
      current_instance->IsRelatedSiteInstance(unrelated_instance.get()));
  {
    SiteInstanceDescriptor descriptor(unrelated_instance.get());
    scoped_refptr<SiteInstance> converted_instance =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    EXPECT_EQ(unrelated_instance.get(), converted_instance);
  }

  // 3) Convert a descriptor of a related instance with the same site as the
  // current one.
  GURL kUrlSameSiteAs1("http://www.a.com/foo");
  {
    SiteInstanceDescriptor descriptor(browser_context(), kUrlSameSiteAs1,
                                      SiteInstanceRelation::RELATED);
    scoped_refptr<SiteInstance> converted_instance =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    EXPECT_EQ(current_instance, converted_instance);
  }

  // 4) Convert a descriptor of a related instance with a site different from
  // the current one.
  GURL kUrlSameSiteAs2("http://www.b.com/foo");
  scoped_refptr<SiteInstance> related_instance;
  {
    SiteInstanceDescriptor descriptor(browser_context(), kUrlSameSiteAs2,
                                      SiteInstanceRelation::RELATED);
    related_instance = ConvertToSiteInstance(rfhm, descriptor, nullptr);
    // Should return a new instance, related to the current, set to the new site
    // URL.
    EXPECT_TRUE(
        current_instance->IsRelatedSiteInstance(related_instance.get()));
    EXPECT_NE(current_instance, related_instance.get());
    EXPECT_NE(unrelated_instance.get(), related_instance.get());
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs2),
              related_instance->GetSiteURL());
  }

  // 5) Convert a descriptor of an unrelated instance with the same site as the
  // current one, several times, with and without candidate sites.
  {
    SiteInstanceDescriptor descriptor(browser_context(), kUrlSameSiteAs1,
                                      SiteInstanceRelation::UNRELATED);
    scoped_refptr<SiteInstance> converted_instance_1 =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    // Should return a new instance, unrelated to the current one, set to the
    // provided site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_1.get()));
    EXPECT_NE(current_instance, converted_instance_1.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_1.get());
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs1),
              converted_instance_1->GetSiteURL());

    // Does the same but this time using unrelated_instance as a candidate,
    // which has a different site.
    scoped_refptr<SiteInstance> converted_instance_2 =
        ConvertToSiteInstance(rfhm, descriptor, unrelated_instance.get());
    // Should return yet another new instance, unrelated to the current one, set
    // to the same site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_2.get()));
    EXPECT_NE(current_instance, converted_instance_2.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_2.get());
    EXPECT_NE(converted_instance_1.get(), converted_instance_2.get());
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs1),
              converted_instance_2->GetSiteURL());

    // Converts once more but with |converted_instance_1| as a candidate.
    scoped_refptr<SiteInstance> converted_instance_3 =
        ConvertToSiteInstance(rfhm, descriptor, converted_instance_1.get());
    // Should return |converted_instance_1| because its site matches and it is
    // unrelated to the current SiteInstance.
    EXPECT_EQ(converted_instance_1.get(), converted_instance_3);
  }

  // 6) Convert a descriptor of an unrelated instance with the same site of
  // related_instance and using it as a candidate.
  {
    SiteInstanceDescriptor descriptor(browser_context(), kUrlSameSiteAs2,
                                      SiteInstanceRelation::UNRELATED);
    scoped_refptr<SiteInstance> converted_instance_1 =
        ConvertToSiteInstance(rfhm, descriptor, related_instance.get());
    // Should return a new instance, unrelated to the current, set to the
    // provided site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_1.get()));
    EXPECT_NE(related_instance.get(), converted_instance_1.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_1.get());
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs2),
              converted_instance_1->GetSiteURL());

    scoped_refptr<SiteInstance> converted_instance_2 =
        ConvertToSiteInstance(rfhm, descriptor, unrelated_instance.get());
    // Should return |unrelated_instance| because its site matches and it is
    // unrelated to the current SiteInstance.
    EXPECT_EQ(unrelated_instance.get(), converted_instance_2);
  }
}

// A renderer process might try and claim that a cross site navigation was
// within the same document by setting was_within_same_document = true in
// FrameHostMsg_DidCommitProvisionalLoad_Params. Such case should be detected on
// the browser side and the renderer process should be killed.
TEST_F(NavigatorTestWithBrowserSideNavigation, CrossSiteClaimWithinPage) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kUrl1);

  // Navigate to a different site and claim that the navigation was within same
  // page.
  int bad_msg_count = process()->bad_msg_count();
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  simulator->CommitSameDocument();
  EXPECT_EQ(process()->bad_msg_count(), bad_msg_count + 1);
}

// Tests that an ongoing NavigationRequest is deleted when a same-site
// user-initiated navigation commits.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       NavigationRequestDeletedWhenUserInitiatedCommits) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/foo");
  const GURL kUrl3("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate same-site.
  int entry_id = RequestNavigation(node, kUrl2);
  main_test_rfh()->PrepareForCommit();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Start a new cross-site navigation. The current RFH should still be trying
  // to commit the previous navigation, but we create a NavigationRequest in the
  // FrameTreeNode.
  RequestNavigation(node, kUrl3);
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_TRUE(node->navigation_request());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // The first navigation commits. This should clear up the speculative RFH and
  // the ongoing NavigationRequest.
  main_test_rfh()->SendNavigate(entry_id, true, kUrl2);
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Tests that an ongoing NavigationRequest is deleted when a cross-site
// navigation commits.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       NavigationRequestDeletedWhenCrossSiteCommits) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");
  const GURL kUrl3("http://www.google.com/foo");

  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate cross-site.
  int entry_id = RequestNavigation(node, kUrl2);
  main_test_rfh()->PrepareForCommit();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Start a new cross-site navigation to the same-site as the ongoing
  // navigation. The speculative RFH should still be live and trying
  // to commit the previous navigation, and we create a NavigationRequest in the
  // FrameTreeNode.
  RequestNavigation(node, kUrl3);
  TestRenderFrameHost* speculative_rfh_2 = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh_2);
  EXPECT_EQ(speculative_rfh_2, speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_TRUE(node->navigation_request());

  // The first navigation commits. This should clear up the speculative RFH and
  // the ongoing NavigationRequest.
  speculative_rfh->SendNavigate(entry_id, true, kUrl2);
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(speculative_rfh, main_test_rfh());
}

// Feature Policy: Test that the feature policy is reset when navigating pages
// within a site.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       FeaturePolicySameSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the feature policy before navigation.
  blink::FeaturePolicy* original_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(original_feature_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the feature policy after navigation.
  blink::FeaturePolicy* final_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(final_feature_policy);
  ASSERT_NE(original_feature_policy, final_feature_policy);
}

// Feature Policy: Test that the feature policy is not reset when navigating
// within a page.
TEST_F(NavigatorTestWithBrowserSideNavigation,
       FeaturePolicyFragmentNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/#Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the feature policy before navigation.
  blink::FeaturePolicy* original_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(original_feature_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the feature policy after navigation.
  blink::FeaturePolicy* final_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_EQ(original_feature_policy, final_feature_policy);
}

// Feature Policy: Test that the feature policy is set correctly when inserting
// a new child frame.
TEST_F(NavigatorTestWithBrowserSideNavigation, FeaturePolicyNewChild) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  TestRenderFrameHost* subframe_rfh =
      contents()->GetMainFrame()->AppendChild("child");
  // Simulate the navigation triggered by inserting a child frame into a page.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  InitNavigateParams(&params, 1, false, kUrl2,
                     ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  subframe_rfh->SendNavigateWithParams(&params, false);

  blink::FeaturePolicy* subframe_feature_policy =
      subframe_rfh->feature_policy();
  ASSERT_TRUE(subframe_feature_policy);
  ASSERT_FALSE(subframe_feature_policy->GetOriginForTest().opaque());
}

TEST_F(NavigatorTestWithBrowserSideNavigation, TwoNavigationsRacingCommit) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  EXPECT_EQ(0u, contents()->GetMainFrame()->navigation_requests_.size());

  // Have the first navigation reach ReadyToCommit.
  auto first_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  first_navigation->ReadyToCommit();
  EXPECT_EQ(1u, contents()->GetMainFrame()->navigation_requests_.size());

  // A second navigation starts and reaches ReadyToCommit.
  auto second_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  second_navigation->ReadyToCommit();
  EXPECT_EQ(2u, contents()->GetMainFrame()->navigation_requests_.size());

  // The first navigation commits.
  first_navigation->Commit();
  EXPECT_EQ(1u, contents()->GetMainFrame()->navigation_requests_.size());

  // The second navigation commits.
  second_navigation->Commit();
  EXPECT_EQ(0u, contents()->GetMainFrame()->navigation_requests_.size());
}

}  // namespace content
