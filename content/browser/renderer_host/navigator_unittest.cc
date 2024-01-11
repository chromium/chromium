// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigator.h"

#include <stdint.h>

#include "base/feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/task_runner_deferring_throttle.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Helper function that determines if a test should expect a cross-site
// navigation to trigger a SiteInstance change based on the current process
// model.
bool ExpectSiteInstanceChange(SiteInstanceImpl* site_instance) {
  return AreAllSitesIsolatedForTesting() ||
         CanCrossSiteNavigationsProactivelySwapBrowsingInstances() ||
         !site_instance->IsDefaultSiteInstance();
}

// Same as above but does not return true if back/forward cache is the only
// trigger for SiteInstance change. This function is useful if, e.g. the test
// intends to disable back/forward cache.
bool ExpectSiteInstanceChangeWithoutBackForwardCache(
    SiteInstanceImpl* site_instance) {
  return AreAllSitesIsolatedForTesting() ||
         !site_instance->IsDefaultSiteInstance();
}

}  // namespace

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

  scoped_refptr<SiteInstanceImpl> ConvertToSiteInstance(
      RenderFrameHostManager* rfhm,
      const SiteInstanceDescriptor& descriptor,
      SiteInstance* candidate_instance) {
    return static_cast<SiteInstanceImpl*>(
        rfhm->ConvertToSiteInstance(
                descriptor, static_cast<SiteInstanceImpl*>(candidate_instance))
            .get());
  }

  SiteInfo CreateExpectedSiteInfo(const GURL& url) {
    return SiteInfo::CreateForTesting(IsolationContext(browser_context()), url);
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
  auto site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();
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
  EXPECT_TRUE(main_test_rfh()->IsActive());
  EXPECT_EQ(main_test_rfh()->lifecycle_state(),
            RenderFrameHostImpl::LifecycleStateImpl::kActive);
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(CreateExpectedSiteInfo(kUrl),
              main_test_rfh()->GetSiteInstance()->GetSiteInfo());
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
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a renderer-initiated non-user-initiated navigation.
  EXPECT_FALSE(node->navigation_request());
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->SetTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT));
  navigation->SetHasUserGesture(false);
  navigation->Start();
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
  EXPECT_TRUE(main_test_rfh()->IsActive());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(CreateExpectedSiteInfo(kUrl2),
              main_test_rfh()->GetSiteInstance()->GetSiteInfo());
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
  scoped_refptr<SiteInstanceImpl> site_instance_1 =
      main_test_rfh()->GetSiteInstance();
  bool expect_site_instance_change =
      ExpectSiteInstanceChange(site_instance_1.get());
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Start a renderer-initiated navigation.
  EXPECT_FALSE(node->navigation_request());
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation->Start();
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);

  // The navigation is immediately started as there's no need to wait for
  // beforeUnload to be executed.
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, request->state());
  EXPECT_EQ(kUrl2, request->common_params().url);
  EXPECT_FALSE(request->browser_initiated());
  if (expect_site_instance_change) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Have the current RenderFrameHost commit the navigation.
  navigation->ReadyToCommit();
  if (expect_site_instance_change) {
    EXPECT_EQ(navigation->GetFinalRenderFrameHost(),
              GetSpeculativeRenderFrameHost(node));
  }
  EXPECT_FALSE(node->navigation_request());

  // Commit the navigation.
  navigation->Commit();
  EXPECT_TRUE(main_test_rfh()->IsActive());
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  if (expect_site_instance_change) {
    EXPECT_NE(site_instance_1->GetId(),
              main_test_rfh()->GetSiteInstance()->GetId());
    EXPECT_EQ(site_instance_1->IsDefaultSiteInstance(),
              main_test_rfh()->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(site_instance_1->GetId(),
              main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// Tests that when a navigation to about:blank is renderer-aborted,
// after another cross-site navigation has been initiated, that the
// second navigation is undisturbed.
TEST_F(NavigatorTest, RendererAbortedAboutBlankNavigation) {
  const GURL kUrl0("http://www.google.com/");
  const GURL kUrl1("about:blank");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl0);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // The test expects cross-site navigations to change RenderFrameHosts, but not
  // same-site navigations. Return if that can't be satisfied.
  DisableBackForwardCacheForTesting(
      contents(), BackForwardCache::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);
  if (!ExpectSiteInstanceChangeWithoutBackForwardCache(
          main_test_rfh()->GetSiteInstance()) ||
      ShouldCreateNewHostForAllFrames()) {
    GTEST_SKIP();
  }

  // Start a renderer-initiated navigation to about:blank.
  EXPECT_FALSE(main_test_rfh()->is_loading());
  auto navigation1 =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  navigation1->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation1->Start();
  navigation1->ReadyToCommit();

  // about:blank should load on the main rfhi, not a speculative one,
  // and automatically advance to READY_TO_COMMIT since it requires
  // no network resources.
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  ASSERT_FALSE(node->navigation_request());
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Start a second, cross-origin navigation.
  auto navigation2 =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  navigation2->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation2->Start();
  ASSERT_TRUE(node->navigation_request());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));

  // Abort the initial navigation.
  navigation1->AbortFromRenderer();

  // But the speculative rfhi and second navigation request
  // should be unaffected.
  ASSERT_TRUE(node->navigation_request());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
}

// Tests that when a navigation to about:blank is renderer-aborted,
// after another cross-site navigation has been initiated, that the
// second navigation is undisturbed. In this variation, the second
// navigation is initially same-site, then redirects cross-site,
// and a throttle DEFERs during WillProcessResponse(). The initial
// navigation gets aborted during this defer.
TEST_F(NavigatorTest,
       RedirectedRendererAbortedAboutBlankNavigationwithDeferredCommit) {
  const GURL kUrl0("http://www.google.com/");
  const GURL kUrl0SameSiteVariation("http://www.google.com/home");
  const GURL kUrl1("about:blank");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl0);
  EXPECT_TRUE(main_test_rfh()->IsRenderFrameLive());

  // The test expects cross-site navigations to change RenderFrameHosts, but not
  // same-site navigations. Return if that can't be satisfied.
  DisableBackForwardCacheForTesting(
      contents(), BackForwardCache::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);
  if (!ExpectSiteInstanceChangeWithoutBackForwardCache(
          main_test_rfh()->GetSiteInstance()) ||
      ShouldCreateNewHostForAllFrames()) {
    GTEST_SKIP();
  }

  // Start a renderer-initiated navigation to about:blank.
  EXPECT_FALSE(main_test_rfh()->is_loading());
  auto navigation1 =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  navigation1->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation1->Start();
  navigation1->ReadyToCommit();

  // about:blank should load on the main rfhi, not a speculative one,
  // and automatically advance to READY_TO_COMMIT since it requires
  // no network resources.
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  ASSERT_FALSE(node->navigation_request());
  EXPECT_TRUE(main_test_rfh()->is_loading());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Start a second, same-origin navigation.
  auto navigation2 = NavigationSimulator::CreateRendererInitiated(
      kUrl0SameSiteVariation, main_test_rfh());
  navigation2->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation2->SetAutoAdvance(false);

  // Insert a TaskRunnerDeferringThrottle that will defer
  // during WillProcessResponse() of navigation2.
  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto* raw_runner = task_runner.get();
  TestNavigationThrottleInserter throttle_inserter(
      web_contents(),
      base::BindRepeating(&TaskRunnerDeferringThrottle::Create,
                          std::move(task_runner), false /* defer_start */,
                          false /* defer-redirect */,
                          true /* defer_response */));

  navigation2->Start();
  ASSERT_TRUE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Redirect navigation2 cross-site, which, once ReadyCommit()
  // is called, will force a speculative RFHI to be created,
  // and update the associated site instance type of the
  // NavigationRequest for navigation2. This will prevent
  // the abort of navigation1 from destroying the speculative
  // RFHI that navigation2 depends on.
  navigation2->Redirect(kUrl2);
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  EXPECT_TRUE(node->navigation_request()->GetAssociatedRFHType() ==
              NavigationRequest::AssociatedRenderFrameHostType::CURRENT);

  navigation2->ReadyToCommit();
  EXPECT_EQ(1u, raw_runner->NumPendingTasks());
  EXPECT_TRUE(navigation2->IsDeferred());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  EXPECT_EQ(node->navigation_request()->GetRenderFrameHost(),
            GetSpeculativeRenderFrameHost(node));

  // Abort the initial navigation.
  navigation1->AbortFromRenderer();

  // The speculative rfhi and second navigation request
  // should be unaffected.
  ASSERT_TRUE(node->navigation_request());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
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
  FrameTreeNode* root_node = contents()->GetPrimaryFrameTree().root();
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
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kSubFrame,
                                 url::Origin::Create(kUrl1),
                                 url::Origin::Create(kUrl2),
                                 net::SiteForCookies::FromUrl(kUrl1))
          .IsEqualForTesting(subframe_loader->request_info()->isolation_info));

  EXPECT_FALSE(subframe_loader->request_info()->is_main_frame);
  EXPECT_TRUE(subframe_request->browser_initiated());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(root_node));

  // Subframe navigations should never create a speculative RenderFrameHost,
  // unless site-per-process or ProcessSharingWithStrictSiteInstances is
  // enabled. In that case, as the subframe navigation is to a different site
  // and is still ongoing, it should have one.
  bool expect_site_instance_change = AreStrictSiteInstancesEnabled();
  if (expect_site_instance_change) {
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
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kMainFrame,
                                 url::Origin::Create(kUrl3),
                                 url::Origin::Create(kUrl3),
                                 net::SiteForCookies::FromUrl(kUrl3))
          .IsEqualForTesting(main_loader->request_info()->isolation_info));
  EXPECT_TRUE(main_loader->request_info()->is_main_frame);
  EXPECT_TRUE(main_request->browser_initiated());
  // BeforeUnloadCompleted callback was invoked by the renderer so the
  // navigation should have started.
  EXPECT_EQ(NavigationRequest::WILL_START_REQUEST, main_request->state());
  EXPECT_TRUE(GetSpeculativeRenderFrameHost(root_node));

  // As the main frame hasn't yet committed the subframe still exists. Thus, the
  // above situation regarding subframe navigations is valid here.
  if (expect_site_instance_change) {
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
  EXPECT_FALSE(node->navigation_request());
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
      std::string(kNoContentHeaders, std::size(kNoContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(std::move(response),
                              mojo::ScopedDataPipeConsumerHandle(),
                              std::nullopt);

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
  EXPECT_FALSE(node->navigation_request());
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now, repeat the test with 205 Reset Content.

  // Navigate to a different site again.
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
      std::string(kResetContentHeaders, std::size(kResetContentHeaders)));
  GetLoaderForNavigationRequest(main_request)
      ->CallOnResponseStarted(std::move(response),
                              mojo::ScopedDataPipeConsumerHandle(),
                              std::nullopt);

  // There should be no pending nor speculative RenderFrameHost; the navigation
  // was aborted.
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
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // It then redirects to another site.
  navigation->Redirect(kUrl2);

  // The redirect should have been followed.
  EXPECT_EQ(1, GetLoaderForNavigationRequest(main_request)->redirect_count());
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

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
  const auto kUrl1SiteInfo = CreateExpectedSiteInfo(kUrl1);
  const GURL kUrl2("http://www.google.com/");
  const auto kUrl2SiteInfo = CreateExpectedSiteInfo(kUrl2);

  // Initialization.
  contents()->NavigateAndCommit(kUrl0);
  FrameTreeNode* node = main_test_rfh()->frame_tree_node();

  // Request navigation to the 1st URL.
  EXPECT_FALSE(node->navigation_request());
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
  auto site_instance_id_1 = speculative_rfh->GetSiteInstance()->GetId();
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(kUrl1SiteInfo, speculative_rfh->GetSiteInstance()->GetSiteInfo());
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
  auto site_instance_id_2 = speculative_rfh->GetSiteInstance()->GetId();

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
    EXPECT_EQ(kUrl2SiteInfo, main_test_rfh()->GetSiteInstance()->GetSiteInfo());
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
  bool expect_site_instance_change =
      ExpectSiteInstanceChange(main_test_rfh()->GetSiteInstance());

  // Start a browser-initiated navigation to the 1st URL and invoke its
  // beforeUnload completion callback.
  EXPECT_FALSE(node->navigation_request());
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
  if (expect_site_instance_change) {
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
  bool expect_site_instance_change =
      ExpectSiteInstanceChange(main_test_rfh()->GetSiteInstance());

  // Start a renderer-initiated user-initiated navigation to the 1st URL.
  EXPECT_FALSE(node->navigation_request());
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
  if (expect_site_instance_change) {
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
  if (expect_site_instance_change) {
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
  EXPECT_FALSE(node->navigation_request());
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
  auto site_instance_id_0 = main_test_rfh()->GetSiteInstance()->GetId();
  bool expect_site_instance_change =
      ExpectSiteInstanceChange(main_test_rfh()->GetSiteInstance());

  // Start a renderer-initiated non-user-initiated navigation to the 1st URL.
  EXPECT_FALSE(node->navigation_request());
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
  if (expect_site_instance_change) {
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
  if (expect_site_instance_change) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  // Confirm that the first loader got destroyed.
  EXPECT_FALSE(loader1);

  // Commit the navigation.
  navigation2->Commit();
  EXPECT_EQ(kUrl2, contents()->GetLastCommittedURL());

  if (expect_site_instance_change) {
    EXPECT_NE(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  } else {
    EXPECT_EQ(site_instance_id_0, main_test_rfh()->GetSiteInstance()->GetId());
  }
}

// PlzNavigate: Test that a reload navigation is properly signaled to the
// RenderFrame when the navigation can commit. A speculative RenderFrameHost
// should not be created at any step, unless RenderDocument is enabled.
TEST_F(NavigatorTest, Reload) {
  const GURL kUrl("http://www.google.com/");
  contents()->NavigateAndCommit(kUrl);

  FrameTreeNode* node = main_test_rfh()->frame_tree_node();
  controller().Reload(ReloadType::NORMAL, false);
  auto reload1 =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  // A NavigationRequest should have been generated.
  NavigationRequest* main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(blink::mojom::NavigationType::RELOAD,
            main_request->common_params().navigation_type);
  reload1->ReadyToCommit();
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }

  reload1->Commit();
  EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));

  // Now do a shift+reload.
  controller().Reload(ReloadType::BYPASSING_CACHE, false);
  auto reload2 =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  // A NavigationRequest should have been generated.
  main_request = node->navigation_request();
  ASSERT_TRUE(main_request != nullptr);
  EXPECT_EQ(blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE,
            main_request->common_params().navigation_type);
  reload2->ReadyToCommit();
  if (ShouldCreateNewHostForAllFrames()) {
    EXPECT_TRUE(GetSpeculativeRenderFrameHost(node));
  } else {
    EXPECT_FALSE(GetSpeculativeRenderFrameHost(node));
  }
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
  EXPECT_FALSE(node->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->Start();
  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  auto site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  ASSERT_TRUE(speculative_rfh);
  EXPECT_NE(speculative_rfh, main_test_rfh());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(CreateExpectedSiteInfo(kUrl),
              speculative_rfh->GetSiteInstance()->GetSiteInfo());
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
  auto init_site_instance_id = main_test_rfh()->GetSiteInstance()->GetId();

  // Begin navigating to another site.
  const GURL kUrl("http://google.com/");
  EXPECT_FALSE(node->navigation_request());
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl, contents());
  navigation->Start();

  TestRenderFrameHost* speculative_rfh = GetSpeculativeRenderFrameHost(node);
  ASSERT_TRUE(speculative_rfh);
  auto site_instance_id = speculative_rfh->GetSiteInstance()->GetId();
  RenderFrameDeletedObserver rfh_deleted_observer(speculative_rfh);
  EXPECT_NE(init_site_instance_id, site_instance_id);
  EXPECT_EQ(init_site_instance_id, main_test_rfh()->GetSiteInstance()->GetId());
  EXPECT_NE(speculative_rfh, main_test_rfh());

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_rfh->GetSiteInstance()->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(CreateExpectedSiteInfo(kUrl),
              speculative_rfh->GetSiteInstance()->GetSiteInfo());
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

  auto redirect_site_instance_id = speculative_rfh->GetSiteInstance()->GetId();

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
    EXPECT_EQ(CreateExpectedSiteInfo(kUrlRedirect),
              speculative_rfh->GetSiteInstance()->GetSiteInfo());
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
  ChildProcessSecurityPolicy::GetInstance()->AddFutureIsolatedOrigins(
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
  ChildProcessSecurityPolicy::GetInstance()->AddFutureIsolatedOrigins(
      {url::Origin::Create(kUrl1)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
      browser_context());
  contents()->NavigateAndCommit(kUrl1);
  SiteInstanceImpl* current_instance = main_test_rfh()->GetSiteInstance();
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
  scoped_refptr<SiteInstanceImpl> unrelated_instance(
      SiteInstanceImpl::CreateForURL(browser_context(), kUrl2));
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
        SiteInstanceRelation::RELATED);
    scoped_refptr<SiteInstance> converted_instance =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    EXPECT_EQ(current_instance, converted_instance);
  }

  // 4) Convert a descriptor of a related instance with a site different from
  // the current one.
  GURL kUrlSameSiteAs2("http://www.b.com/foo");
  scoped_refptr<SiteInstanceImpl> related_instance;
  {
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs2),
        SiteInstanceRelation::RELATED);
    related_instance = ConvertToSiteInstance(rfhm, descriptor, nullptr);
    // If kUrlSameSiteAs2 requires a dedicated process on this platform, this
    // should return a new instance, related to the current and set to the new
    // site URL.
    // Otherwise, this should return the default site instance
    EXPECT_TRUE(
        current_instance->IsRelatedSiteInstance(related_instance.get()));
    EXPECT_NE(current_instance, related_instance.get());
    EXPECT_NE(unrelated_instance.get(), related_instance.get());

    if (AreDefaultSiteInstancesEnabled()) {
      ASSERT_TRUE(related_instance->IsDefaultSiteInstance());
    } else {
      EXPECT_EQ(SiteInfo::CreateForTesting(
                    current_instance->GetIsolationContext(), kUrlSameSiteAs2),
                related_instance->GetSiteInfo());
    }
  }

  // 5) Convert a descriptor of an unrelated instance with the same site as the
  // current one, several times, with and without candidate sites.
  {
    SiteInstanceDescriptor descriptor(
        UrlInfo::CreateForTesting(kUrlSameSiteAs1),
        SiteInstanceRelation::UNRELATED);
    scoped_refptr<SiteInstanceImpl> converted_instance_1 =
        ConvertToSiteInstance(rfhm, descriptor, nullptr);
    // Should return a new instance, unrelated to the current one, set to the
    // provided site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_1.get()));
    EXPECT_NE(current_instance, converted_instance_1.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_1.get());
    EXPECT_EQ(CreateExpectedSiteInfo(kUrlSameSiteAs1),
              converted_instance_1->GetSiteInfo());

    // Does the same but this time using unrelated_instance as a candidate,
    // which has a different site.
    scoped_refptr<SiteInstanceImpl> converted_instance_2 =
        ConvertToSiteInstance(rfhm, descriptor, unrelated_instance.get());
    // Should return yet another new instance, unrelated to the current one, set
    // to the same site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_2.get()));
    EXPECT_NE(current_instance, converted_instance_2.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_2.get());
    EXPECT_NE(converted_instance_1.get(), converted_instance_2.get());
    EXPECT_EQ(CreateExpectedSiteInfo(kUrlSameSiteAs1),
              converted_instance_1->GetSiteInfo());

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
        SiteInstanceRelation::UNRELATED);
    scoped_refptr<SiteInstanceImpl> converted_instance_1 =
        ConvertToSiteInstance(rfhm, descriptor, related_instance.get());
    // Should return a new instance, unrelated to the current, set to the
    // provided site URL.
    EXPECT_FALSE(
        current_instance->IsRelatedSiteInstance(converted_instance_1.get()));
    EXPECT_NE(related_instance.get(), converted_instance_1.get());
    EXPECT_NE(unrelated_instance.get(), converted_instance_1.get());

    if (AreDefaultSiteInstancesEnabled()) {
      EXPECT_TRUE(converted_instance_1->IsDefaultSiteInstance());
    } else {
      EXPECT_EQ(CreateExpectedSiteInfo(kUrlSameSiteAs2),
                converted_instance_1->GetSiteInfo());
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
// DidCommitProvisionalLoadParams. Such case should be detected on the browser
// side and the renderer process should be killed.
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

// Permissions Policy: Test that the permissions policy is reset when navigating
// pages within a site.
TEST_F(NavigatorTest, PermissionsPolicySameSiteNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the permissions policy before navigation.
  const blink::PermissionsPolicy* original_permissions_policy =
      main_test_rfh()->permissions_policy();
  ASSERT_TRUE(original_permissions_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the permissions policy after navigation.
  const blink::PermissionsPolicy* final_permissions_policy =
      main_test_rfh()->permissions_policy();
  ASSERT_TRUE(final_permissions_policy);
  ASSERT_NE(original_permissions_policy, final_permissions_policy);
}

// Permissions Policy: Test that the permissions policy is not reset when
// navigating within a page.
TEST_F(NavigatorTest, PermissionsPolicyFragmentNavigation) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/#Home");

  contents()->NavigateAndCommit(kUrl1);

  // Check the permissions policy before navigation.
  const blink::PermissionsPolicy* original_permissions_policy =
      main_test_rfh()->permissions_policy();
  ASSERT_TRUE(original_permissions_policy);

  // Navigate to the new URL.
  contents()->NavigateAndCommit(kUrl2);

  // Check the permissions policy after navigation.
  const blink::PermissionsPolicy* final_permissions_policy =
      main_test_rfh()->permissions_policy();
  ASSERT_EQ(original_permissions_policy, final_permissions_policy);
}

// Permissions Policy: Test that the permissions policy is set correctly when
// inserting a new child frame.
TEST_F(NavigatorTest, PermissionsPolicyNewChild) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  contents()->NavigateAndCommit(kUrl1);

  // Simulate the navigation triggered by inserting a child frame into a page.
  TestRenderFrameHost* subframe_rfh =
      contents()->GetPrimaryMainFrame()->AppendChild("child");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, subframe_rfh);

  const blink::PermissionsPolicy* subframe_permissions_policy =
      subframe_rfh->permissions_policy();
  ASSERT_TRUE(subframe_permissions_policy);
  ASSERT_FALSE(subframe_permissions_policy->GetOriginForTest().opaque());
}

TEST_F(NavigatorTest, TwoNavigationsRacingCommit) {
  const GURL kUrl1("http://www.chromium.org/");
  const GURL kUrl2("http://www.chromium.org/Home");

  EXPECT_EQ(0u, contents()->GetPrimaryMainFrame()->navigation_requests_.size());

  // Have the first navigation reach ReadyToCommit.
  auto first_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  first_navigation->ReadyToCommit();
  EXPECT_EQ(1u, contents()->GetPrimaryMainFrame()->navigation_requests_.size());

  // A second navigation starts.
  auto second_navigation =
      NavigationSimulator::CreateBrowserInitiated(kUrl1, contents());
  second_navigation->Start();
  EXPECT_EQ(1u, contents()->GetPrimaryMainFrame()->navigation_requests_.size());

  // The first navigation commits.
  first_navigation->Commit();
  EXPECT_EQ(0u, contents()->GetPrimaryMainFrame()->navigation_requests_.size());

  // The second navigation commits.
  second_navigation->Commit();
  EXPECT_EQ(0u, contents()->GetPrimaryMainFrame()->navigation_requests_.size());
}

}  // namespace content
