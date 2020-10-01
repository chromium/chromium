// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigator.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

class NavigatorTest : public RenderViewHostImplTestHarness {
 public:
  using SiteInstanceDescriptor = RenderFrameHostManager::SiteInstanceDescriptor;
  using SiteInstanceRelation = RenderFrameHostManager::SiteInstanceRelation;

  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  void TearDown() override { RenderViewHostImplTestHarness::TearDown(); }

  TestNavigationURLLoader* GetLoaderForNavigationRequest(
      NavigationRequest* request) const {
    return static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  }

  TestRenderFrameHost* GetSpeculativeRenderFrameHost(FrameTreeNode* node) {
    return static_cast<TestRenderFrameHost*>(
        node->render_manager()->speculative_render_frame_host_.get());
  }

  scoped_refptr<SiteInstance> ConvertToSiteInstance(
      RenderFrameHostManager* rfhm,
      const SiteInstanceDescriptor& descriptor,
      SiteInstance* candidate_instance) {
    return rfhm->ConvertToSiteInstance(
        descriptor, static_cast<SiteInstanceImpl*>(candidate_instance),
        false /* is_speculative */);
  }
};

// Tests a complete browser-initiated navigation starting with a non-live
// renderer.
TEST_F(NavigatorTest, SimpleBrowserInitiatedNavigationFromNonLiveRenderer) {
  const GURL kUrl("http://chromium.org/");

  EXPECT_FALSE(main_test_rfh()->IsRenderFrameLive());

  // Start a browser-initiated navigation.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  int32_t site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  navigation->Start();
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(kUrl, request->common_params().url);
  EXPECT_TRUE(request->browser_initiated());

  // As there's no live renderer the navigation should not wait for a
  // beforeUnload completion callback being invoked by the renderer and
  // start right away.
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());
  ASSERT_TRUE(GetLoaderForNavigationRequest(request));
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  navigation->ReadyToCommit();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->IsCurrent());
  EXPECT_EQ(main_test_rfh()->lifecycle_state(),
            RenderFrameHostImpl::LifecycleState::kActive);
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
              main_test_rfh()->GetSiteInstance()->GetSiteURL());
  }
  EXPECT_EQ(kUrl, contents()->GetLastCommittedURL());

  // The main RenderFrameHost should not have been changed, and the renderer
  // should have been initialized.
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // After a navigation is finished no speculative RenderFrameHost should
  // exist.
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Tests a complete renderer-initiated same-site navigation.
TEST_F(NavigatorTest, SimpleRendererInitiatedSameSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());
  static_cast<mojom::FrameHost*>(main_test_rfh())->DidStopLoading();

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
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());
  EXPECT_FALSE(request->common_params().has_user_gesture);
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());

  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    // If same-site ProactivelySwapBrowsingInstance or main-frame RenderDocument
    // is enabled, the RFH should change so we should have a speculative RFH.
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node)->is_loading());
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }
  EXPECT_FALSE(main_test_rfh()->is_loading());

  // Have the current RenderFrameHost commit the navigation
  navigation->ReadyToCommit();
  if (CanSameSiteMainFrameNavigationsChangeRenderFrameHosts()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node)->is_loading());
  } else {
    EXPECT_TRUE(main_test_rfh()->is_loading());
  }
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->IsCurrent());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl2),
              main_test_rfh()->GetSiteInstance()->GetSiteURL());
  }
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Tests a complete renderer-initiated navigation that should be
// cross-site but does not result in a SiteInstance swap because its
// renderer-initiated.
TEST_F(NavigatorTest, SimpleRendererInitiatedCrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com");

  contents()->NavigateAndCommit(kUrl1);
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
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Have the current RenderFrameHost commit the navigation.
  navigation->ReadyToCommit();
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_EQ(navigation->GetFinalRenderFrameHost(),
              GetSpeculativeRenderFrameHost(node));
  }
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->IsCurrent());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // The SiteInstance did not change unless site-per-process is enabled.
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_NE(site_instance_id_1, main_test_rfh()->GetSiteInstance()->GetId());
  } else {
    EXPECT_EQ(site_instance_id_1, main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// Tests that a beforeUnload denial cancels the navigation.
TEST_F(NavigatorTest, BeforeUnloadDenialCancelNavigation) {
  const GURL kUrl1("http://www.google.com/");
  const GURL kUrl2("http://www.chromium.org/");

  contents()->NavigateAndCommit(kUrl1);

  // Start a new navigation.
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  navigation->BrowserInitiatedStartAndWaitBeforeUnload();
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);
  EXPECT_TRUE(request->browser_initiated());
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE, request->state());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  RenderFrameDeletedObserver rfh_deleted_observer(
      GetSpeculativeRenderFrameHost(node));

  // Simulate a beforeUnload denial.
  main_test_rfh()->SimulateBeforeUnloadCompleted(false);
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_TRUE(rfh_deleted_observer.deleted());
}

// Test that a proper NavigationRequest is created at navigation start.
TEST_F(NavigatorTest, BeginNavigation) {
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
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl2, contents());
  NavigationController::LoadURLParams load_url_params(kUrl2);
  load_url_params.frame_tree_node_id = subframe_node->frame_tree_node_id();
  navigation->SetLoadURLParams(&load_url_params);
  navigation->BrowserInitiatedStartAndWaitBeforeUnload();
  NavigationRequest* subframe_request = subframe_node->navigation_request();

  // We should be waiting for the BeforeUnload event to execute in the subframe.
  ASSERT_TRUE(subframe_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            subframe_request->state());
  EXPECT_TRUE(subframe_rfh->is_waiting_for_beforeunload_completion());

  // Start the navigation, which will internally simulate that the beforeUnload
  // completion callback has been invoked.
  navigation->Start();
  TestNavigationURLLoader* subframe_loader =
      GetLoaderForNavigationRequest(subframe_request);
  ASSERT_TRUE(subframe_loader);
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, subframe_request->state());
  EXPECT_EQ(kUrl2, subframe_request->common_params().url);
  EXPECT_EQ(kUrl2, subframe_loader->request_info()->common_params->url);
  EXPECT_TRUE(
      net::IsolationInfo::Create(
          net::IsolationInfo::RedirectMode::kUpdateFrameOnly,
          url::Origin::Create(kUrl1), url::Origin::Create(kUrl2),
          net::SiteForCookies::FromUrl(kUrl1))
          .IsEqualForTesting(subframe_loader->request_info()->isolation_info));

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
  auto navigation2 =
      NavigationSimulatorImpl::CreateBrowserInitiated(kUrl3, contents());
  navigation2->BrowserInitiatedStartAndWaitBeforeUnload();
  NavigationRequest* main_request = root_node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_EQ(NavigationRequest::WAITING_FOR_RENDERER_RESPONSE,
            main_request->state());

  // Main frame navigation to a different site should use a speculative
  // RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // Start the navigation, which will internally simulate that the beforeUnload
  // completion callback has been invoked.
  navigation2->Start();
  TestNavigationURLLoader* main_loader =
      GetLoaderForNavigationRequest(main_request);
  EXPECT_EQ(kUrl3, main_request->common_params().url);
  EXPECT_EQ(kUrl3, main_loader->request_info()->common_params->url);
  EXPECT_TRUE(
      net::IsolationInfo::Create(
          net::IsolationInfo::RedirectMode::kUpdateTopFrame,
          url::Origin::Create(kUrl3), url::Origin::Create(kUrl3),
          net::SiteForCookies::FromUrl(kUrl3))
          .IsEqualForTesting(main_loader->request_info()->isolation_info));
  EXPECT_TRUE(main_loader->request_info()->is_main_frame);
  EXPECT_FALSE(main_loader->request_info()->parent_is_main_frame);
  EXPECT_TRUE(main_request->browser_initiated());
  // BeforeUnloadCompleted callback was invoked by the renderer so the
  // navigation should have started.
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, main_request->state());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // As the main frame hasn't yet committed the subframe still exists. Thus, the
  // above situation regarding subframe navigations is valid here.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(subframe_node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(subframe_node));
  }
}

// Tests that committing an HTTP 204 or HTTP 205 response cancels
// the navigation.
TEST_F(NavigatorTest, NoContent) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Load a URL.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->Start();

  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);

  // Navigations to a different site do create a speculative RenderFrameHost.
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 204 response.
  auto response = network::mojom::URLResponseHead::New();
  const char kNoContentHeaders[] = "HTTP/1.1 204 No Content\0\0";
  response->headers = new net::HttpResponseHeaders(
      std::string(kNoContentHeaders, base::size(kNoContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(std::move(response));

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now, repeat the test with 205 Reset Content.

  // Navigate to a different site again.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation2->Start();

  main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Commit an HTTP 205 response.
  response = network::mojom::URLResponseHead::New();
  const char kResetContentHeaders[] = "HTTP/1.1 205 Reset Content\0\0";
  response->headers = new net::HttpResponseHeaders(
      std::string(kResetContentHeaders, base::size(kResetContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(std::move(response));

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Test that a new RenderFrameHost is created when doing a cross site
// navigation.
TEST_F(NavigatorTest, CrossSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* initial_rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a different site.
  EXPECT_EQ(main_test_rfh()->navigation_requests().size(), 0u);
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->Start();
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));

  navigation->ReadyToCommit();
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(speculative_rfh->navigation_requests().size(), 1u);
  EXPECT_EQ(main_test_rfh()->navigation_requests().size(), 0u);

  navigation->Commit();
  RenderFrameHostImpl* final_rfh = main_test_rfh();
  EXPECT_EQ(speculative_rfh, final_rfh);
  EXPECT_NE(initial_rfh, final_rfh);
  EXPECT_TRUE(final_rfh->IsRenderFrameLive());
  EXPECT_TRUE(final_rfh->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Test that redirects are followed and the speculative RenderFrameHost logic
// behaves as expected.
TEST_F(NavigatorTest, RedirectCrossSite) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  RenderFrameHostImpl* rfh = main_test_rfh();
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate to a URL on the same site.
  EXPECT_EQ(main_test_rfh()->navigation_requests().size(), 0u);
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  navigation->Start();
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // It then redirects to another site.
  navigation->Redirect(kUrl2);

  // The redirect should have been followed.
  EXPECT_EQ(1, GetLoaderForNavigationRequest(main_request)->redirect_count());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  navigation->ReadyToCommit();
  TestRenderFrameHost* final_speculative_rfh =
      GetSpeculativeRenderFrameHost(node);
  EXPECT_TRUE(final_speculative_rfh);
  EXPECT_EQ(final_speculative_rfh->navigation_requests().size(), 1u);

  navigation->Commit();
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
TEST_F(NavigatorTest, BrowserInitiatedNavigationCancel) {
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
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  navigation1->Start();
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
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kUrl1_site, speculative_rfh->GetSiteInstance()->GetSiteURL());
  }

  // Request navigation to the 2nd URL; the NavigationRequest must have been
  // replaced by a new one with a different URL.
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation2->Start();
  NavigationRequest* request2 = node->navigation_request();
  ASSERT_TRUE(request2);
  EXPECT_EQ(kUrl2, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Confirm that a new speculative RenderFrameHost was created.
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32_t site_instance_id_2 = speculative_rfh->GetSiteInstance()->GetId();

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_EQ(site_instance_id_1, site_instance_id_2);
  } else {
    EXPECT_NE(site_instance_id_1, site_instance_id_2);
  }

  navigation2->ReadyToCommit();
  EXPECT_EQ(speculative_rfh->navigation_requests().size(), 1u);
  EXPECT_EQ(main_test_rfh()->navigation_requests().size(), 0u);

  // Have the RenderFrameHost commit the navigation.
  navigation2->Commit();

  // Confirm that the commit corresponds to the new request.
  ASSERT_TRUE(main_test_rfh());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kUrl2_site, main_test_rfh()->GetSiteInstance()->GetSiteURL());
  }
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  // Confirm that the committed RenderFrameHost is the latest speculative one.
  EXPECT_EQ(site_instance_id_2, main_test_rfh()->GetSiteInstance()->GetId());
}

// Test that a browser-initiated navigation is canceled if a renderer-initiated
// user-initiated request has been issued in the meantime.
TEST_F(NavigatorTest, RendererUserInitiatedNavigationCancel) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL and invoke its
  // beforeUnload completion callback.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  navigation2->Start();
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
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

// Tests that a renderer-initiated user-initiated navigation is
// canceled if a renderer-initiated non-user-initiated request is issued in the
// meantime.
TEST_F(NavigatorTest,
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
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
TEST_F(NavigatorTest,
       RendererNonUserInitiatedNavigationDoesntCancelBrowserInitiated) {
  const GURL kUrl0("http://www.wikipedia.org/");
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a browser-initiated navigation to the 1st URL.
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  navigation->Start();
  NavigationRequest* request1 = node->navigation_request();
  ASSERT_TRUE(request1);
  EXPECT_EQ(kUrl1, request1->common_params().url);
  EXPECT_TRUE(request1->browser_initiated());
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);

  // Now receive a renderer-initiated non-user-initiated request. Nothing should
  // change.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(
      kUrl2, false /* has_user_gesture */);
  NavigationRequest* request2 = node->navigation_request();
  ASSERT_TRUE(request2);
  EXPECT_EQ(request1, request2);
  EXPECT_EQ(kUrl1, request2->common_params().url);
  EXPECT_TRUE(request2->browser_initiated());
  EXPECT_TRUE(speculative_rfh);

  navigation->ReadyToCommit();
  EXPECT_EQ(speculative_rfh->navigation_requests().size(), 1u);
  EXPECT_EQ(main_test_rfh()->navigation_requests().size(), 0u);

  navigation->Commit();
  EXPECT_EQ(kUrl1, contents()->GetLastCommittedURL());
}

// PlzNavigate: Test that a renderer-initiated non-user-initiated navigation is
// canceled if a another similar request is issued in the meantime.
TEST_F(NavigatorTest,
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
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
  if (AreAllSitesIsolatedForTesting() ||
      CanCrossSiteNavigationsProactivelySwapBrowsingInstances()) {
    EXPECT_NE(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  } else {
    EXPECT_EQ(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// PlzNavigate: Test that a reload navigation is properly signaled to the
// RenderFrame when the navigation can commit. A speculative RenderFrameHost
// should not be created at any step.
TEST_F(NavigatorTest, Reload) {
  const GURL kUrl("http://www.google.com/");
  contents()->NavigateAndCommit(kUrl);

  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  controller().Reload(ReloadType::NORMAL, false);
  auto reload1 = NavigationSimulator::CreateFromPending(contents());
  // A NavigationRequest should have been generated.
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(mojom::NavigationType::RELOAD,
            main_request->common_params().navigation_type);
  reload1->ReadyToCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  reload1->Commit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now do a shift+reload.
  controller().Reload(ReloadType::BYPASSING_CACHE, false);
  auto reload2 = NavigationSimulator::CreateFromPending(contents());
  // A NavigationRequest should have been generated.
  main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(mojom::NavigationType::RELOAD_BYPASSING_CACHE,
            main_request->common_params().navigation_type);
  reload2->ReadyToCommit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is used when
// navigating from one site to another.
TEST_F(NavigatorTest, SpeculativeRendererWorksBaseCase) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->Start();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  int32_t site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  ASSERT_TRUE(speculative_rfh);
  EXPECT_NE(speculative_rfh, main_test_rfh());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
              speculative_rfh->GetSiteInstance()->GetSiteURL());
  }

  navigation->ReadyToCommit();
  EXPECT_EQ(speculative_rfh->navigation_requests().size(), 1u);

  // Ask the navigation to commit.
  navigation->Commit();
  EXPECT_EQ(site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Confirm that a speculative RenderFrameHost is thrown away when
// the final URL's site differs from the initial one due to redirects.
TEST_F(NavigatorTest, SpeculativeRendererDiscardedAfterRedirectToAnotherSite) {
  // Navigate to an initial site.
  const GURL kUrlInit("http://wikipedia.org/");
  contents()->NavigateAndCommit(kUrlInit);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  int32_t init_site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  EXPECT_FALSE(main_test_rfh()->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->Start();

  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  int32_t site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  RenderFrameDeletedObserver rfh_deleted_observer(speculative_rfh);
  EXPECT_NE(init_site_instance_id, site_instance_id);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_NE(speculative_rfh, main_test_rfh());

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrl),
              speculative_rfh->GetSiteInstance()->GetSiteURL());
  }

  // It then redirects to yet another site.
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request);
  const GURL kUrlRedirect("https://www.google.com/");
  navigation->Redirect(kUrlRedirect);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());

  // For now, ensure that the speculative RenderFrameHost does not change after
  // the redirect.
  // TODO(carlosk): once the speculative RenderFrameHost updates with redirects
  // this next check will be changed to verify that it actually happens.
  EXPECT_EQ(speculative_rfh, GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(site_instance_id, speculative_rfh->GetSiteInstance()->GetId());
  EXPECT_FALSE(rfh_deleted_observer.deleted());

  // Send the commit to the renderer.
  navigation->ReadyToCommit();

  // Once commit happens the speculative RenderFrameHost is updated to match the
  // known final SiteInstance.
  speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_EQ(speculative_rfh->navigation_requests().size(), 1u);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());

  int32_t redirect_site_instance_id =
      speculative_rfh->GetSiteInstance()->GetId();

  // Expect the initial and redirect SiteInstances to be different because
  // they should be associated with different BrowsingInstances.
  EXPECT_NE(init_site_instance_id, redirect_site_instance_id);

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
    EXPECT_EQ(site_instance_id, redirect_site_instance_id);

    // Verify the old speculative RenderFrameHost was not deleted because
    // the SiteInstance stayed the same.
    EXPECT_FALSE(rfh_deleted_observer.deleted());
  } else {
    EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlRedirect),
              speculative_rfh->GetSiteInstance()->GetSiteURL());
    EXPECT_NE(site_instance_id, redirect_site_instance_id);

    // Verify the old speculative RenderFrameHost was deleted because
    // the SiteInstance changed.
    EXPECT_TRUE(rfh_deleted_observer.deleted());
  }

  // Invoke DidCommitProvisionalLoad.
  navigation->Commit();

  // Check that the speculative RenderFrameHost was swapped in.
  EXPECT_EQ(redirect_site_instance_id,
            main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// PlzNavigate: Verify that data urls are properly handled.
TEST_F(NavigatorTest, DataUrls) {
  const GURL kUrl1("http://wikipedia.org/");
  const GURL kUrl2("data:text/html,test");

  // Isolate kUrl1 so it can't be mapped into a default SiteInstance along with
  // kUrl2. This ensures that the speculative RenderFrameHost will always be
  // used because the URLs map to different SiteInstances.
  ChildProcessSecurityPolicy::GetInstance()->AddIsolatedOrigins(
      {url::Origin::Create(kUrl1)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
      browser_context());

  // Navigate to an initial site.
  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  EXPECT_FALSE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());

  // Navigate to a data url. The request should have been sent to the IO
  // thread and not committed immediately.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->Start();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_FALSE(speculative_rfh->is_loading());
  EXPECT_TRUE(node->navigation_request());
  navigation->ReadyToCommit();
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_FALSE(node->navigation_request());
  EXPECT_NE(main_test_rfh(), speculative_rfh);
  navigation->Commit();
  EXPECT_EQ(main_test_rfh(), speculative_rfh);

  // Go back to the initial site.
  contents()->NavigateAndCommit(kUrl1);

  // Do a renderer-initiated navigation to a data url. The request should be
  // sent to the IO thread.
  auto navigation_to_data_url =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation_to_data_url->Start();
  EXPECT_FALSE(main_test_rfh()->is_loading());
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
TEST_F(NavigatorTest, SiteInstanceDescriptionConversion) {
  // Navigate to set a current SiteInstance on the RenderFrameHost.
  GURL kUrl1("http://a.com");
  // Isolate one of the sites so the both can't be mapped to the default
  // site instance.
  ChildProcessSecurityPolicy::GetInstance()->AddIsolatedOrigins(
      {url::Origin::Create(kUrl1)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
      browser_context());
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
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs1),
        SiteInstanceRelation::RELATED,
        false /* is_coop_coep_cross_origin_isolated */);
    scoped_refptr<SiteInstance> converted_instance =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    EXPECT_EQ(current_instance, converted_instance);
  }

  // 4) Convert a descriptor of a related instance with a site different from
  // the current one.
  GURL kUrlSameSiteAs2("http://www.b.com/foo");
  scoped_refptr<SiteInstance> related_instance;
  {
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs2),
        SiteInstanceRelation::RELATED,
        false /* is_coop_coep_cross_origin_isolated */);
    related_instance = ConvertToSiteInstance(rfhm, descriptor, nullptr);
    // If kUrlSameSiteAs2 requires a dedicated process on this platform, this
    // should return a new instance, related to the current and set to the new
    // site URL.
    // Otherwise, this should return the default site instance
    EXPECT_TRUE(
        current_instance->IsRelatedSiteInstance(related_instance.get()));
    EXPECT_NE(current_instance, related_instance.get());
    EXPECT_NE(unrelated_instance.get(), related_instance.get());

    if (AreAllSitesIsolatedForTesting()) {
      EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs2),
                related_instance->GetSiteURL());
    } else {
      EXPECT_TRUE(static_cast<SiteInstanceImpl*>(related_instance.get())
                      ->IsDefaultSiteInstance());
    }
  }

  // 5) Convert a descriptor of an unrelated instance with the same site as the
  // current one, several times, with and without candidate sites.
  {
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs1),
        SiteInstanceRelation::UNRELATED,
        false /* is_coop_coep_cross_origin_isolated */);
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
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs2),
        SiteInstanceRelation::UNRELATED,
        false /* is_coop_coep_cross_origin_isolated */);
    scoped_refptr<SiteInstance> converted_instance_1 =
        ConvertToSiteInstance(rfhm, descriptor, related_instance.get());
    // Should return a new instance, unrelated to the current, set to the
    // provided site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_1.get()));
    EXPECT_NE(related_instance.get(), converted_instance_1.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_1.get());

    if (AreDefaultSiteInstancesEnabled()) {
      EXPECT_TRUE(static_cast<SiteInstanceImpl*>(converted_instance_1.get())
                      ->IsDefaultSiteInstance());
    } else {
      EXPECT_EQ(SiteInstance::GetSiteForURL(browser_context(), kUrlSameSiteAs2),
                converted_instance_1->GetSiteURL());
    }

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
TEST_F(NavigatorTest, CrossSiteClaimWithinPage) {
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
TEST_F(NavigatorTest, NavigationRequestDeletedWhenUserInitiatedCommits) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/foo");
  const GURL kUrl3("http://www.google.com/");

  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // The test below only makes sense if the same-site navigation below will not
  // create a speculative RFH, so we need to ensure that we won't trigger a
  // same-site cross-RFH navigation.
  // Note: this will not disable RenderDocument.
  // TODO(crbug.com/936696): Skip this test when main-frame RenderDocument is
  // enabled.
  DisableProactiveBrowsingInstanceSwapFor(main_test_rfh());

  // Navigate same-site.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Start a new cross-site navigation. The current RFH should still be trying
  // to commit the previous navigation, but we create a NavigationRequest in the
  // FrameTreeNode.
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl3, contents());
  navigation2->Start();
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_TRUE(node->navigation_request());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // The first navigation commits. This should clear up the speculative RFH and
  // the ongoing NavigationRequest.
  navigation->Commit();
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
}

// Tests that an ongoing NavigationRequest is deleted when a cross-site
// navigation commits.
TEST_F(NavigatorTest, NavigationRequestDeletedWhenCrossSiteCommits) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.google.com/");
  const GURL kUrl3("http://www.google.com/foo");

  contents()->NavigateAndCommit(kUrl1);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Navigate cross-site.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->ReadyToCommit();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_FALSE(node->navigation_request());

  // Start a new cross-site navigation to the same-site as the ongoing
  // navigation. The speculative RFH should still be live and trying
  // to commit the previous navigation, and we create a NavigationRequest in the
  // FrameTreeNode.
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(kUrl3, contents());
  navigation2->Start();
  TestRenderFrameHost* speculative_rfh_2 = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh_2);
  EXPECT_EQ(speculative_rfh_2, speculative_rfh);
  EXPECT_TRUE(speculative_rfh->is_loading());
  EXPECT_TRUE(node->navigation_request());

  // The first navigation commits. This should clear up the speculative RFH and
  // the ongoing NavigationRequest.
  navigation->Commit();
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(speculative_rfh, main_test_rfh());
}

// Feature Policy: Test that the feature policy is reset when navigating pages
// within a site.
TEST_F(NavigatorTest, FeaturePolicySameSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the feature policy before navigation.
  const blink::FeaturePolicy* original_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(original_feature_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the feature policy after navigation.
  const blink::FeaturePolicy* final_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(final_feature_policy);
  ASSERT_NE(original_feature_policy, final_feature_policy);
}

// Feature Policy: Test that the feature policy is not reset when navigating
// within a page.
TEST_F(NavigatorTest, FeaturePolicyFragmentNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/#Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the feature policy before navigation.
  const blink::FeaturePolicy* original_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_TRUE(original_feature_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the feature policy after navigation.
  const blink::FeaturePolicy* final_feature_policy =
      main_test_rfh()->feature_policy();
  ASSERT_EQ(original_feature_policy, final_feature_policy);
}

// Feature Policy: Test that the feature policy is set correctly when inserting
// a new child frame.
TEST_F(NavigatorTest, FeaturePolicyNewChild) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  // Simulate the navigation triggered by inserting a child frame into a page.
  TestRenderFrameHost* subframe_rfh =
      contents()->GetMainFrame()->AppendChild("child");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, subframe_rfh);

  const blink::FeaturePolicy* subframe_feature_policy =
      subframe_rfh->feature_policy();
  ASSERT_TRUE(subframe_feature_policy);
  ASSERT_FALSE(subframe_feature_policy->GetOriginForTest().opaque());
}

TEST_F(NavigatorTest, TwoNavigationsRacingCommit) {
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
