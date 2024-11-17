// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/spare_render_process_host_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/test/storage_partition_test_helpers.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"

namespace content {

class RenderProcessHostUnitTest : public RenderViewHostImplTestHarness {
 public:
  scoped_refptr<SiteInstanceImpl> CreateForUrl(const GURL& url) {
    return SiteInstanceImpl::CreateForTesting(browser_context(), url);
  }

  scoped_refptr<SiteInstanceImpl> CreateForServiceWorker(
      const GURL& url,
      bool can_reuse_process = false) {
    return SiteInstanceImpl::CreateForServiceWorker(
        browser_context(),
        UrlInfo::CreateForTesting(url,
                                  CreateStoragePartitionConfigForTesting()),
        can_reuse_process);
  }
};

// Tests that guest RenderProcessHosts are not considered suitable hosts when
// searching for RenderProcessHost.
TEST_F(RenderProcessHostUnitTest, GuestsAreNotSuitableHosts) {
  GURL test_url("http://foo.com");

  MockRenderProcessHost guest_host(browser_context(),
                                   /*is_for_guest_only=*/true);

  scoped_refptr<SiteInstanceImpl> site_instance = CreateForUrl(test_url);
  EXPECT_FALSE(RenderProcessHostImpl::IsSuitableHost(
      &guest_host, site_instance->GetIsolationContext(),
      site_instance->GetSiteInfo()));
  EXPECT_TRUE(RenderProcessHostImpl::IsSuitableHost(
      process(), site_instance->GetIsolationContext(),
      site_instance->GetSiteInfo()));
  EXPECT_EQ(process(),
            RenderProcessHostImpl::GetExistingProcessHost(site_instance.get()));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(RenderProcessHostUnitTest, RendererProcessLimit) {
  // This test shouldn't run with --site-per-process mode, which prohibits
  // the renderer process reuse this test explicitly exercises.
  if (AreAllSitesIsolatedForTesting())
    return;

  const size_t max_renderer_process_count =
      RenderProcessHostImpl::GetPlatformMaxRendererProcessCount();

  // Verify that the limit is between 1 and |max_renderer_process_count|.
  EXPECT_GT(RenderProcessHostImpl::GetMaxRendererProcessCount(), 0u);
  EXPECT_LE(RenderProcessHostImpl::GetMaxRendererProcessCount(),
            max_renderer_process_count);

  // Add dummy process hosts to saturate the limit.
  ASSERT_NE(0u, max_renderer_process_count);
  std::vector<std::unique_ptr<MockRenderProcessHost>> hosts;
  for (size_t i = 0; i < max_renderer_process_count; ++i) {
    hosts.push_back(std::make_unique<MockRenderProcessHost>(browser_context()));
  }

  // Verify that the renderer sharing will happen.
  GURL test_url("http://foo.com");
  EXPECT_TRUE(RenderProcessHostImpl::IsProcessLimitReached());
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(RenderProcessHostUnitTest, NoRendererProcessLimitOnAndroidOrChromeOS) {
  // Add a few dummy process hosts.
  static constexpr size_t kMaxRendererProcessCountForTesting = 82;
  std::vector<std::unique_ptr<MockRenderProcessHost>> hosts;
  for (size_t i = 0; i < kMaxRendererProcessCountForTesting; ++i) {
    hosts.push_back(std::make_unique<MockRenderProcessHost>(browser_context()));
  }

  // Verify that the renderer sharing still won't happen.
  GURL test_url("http://foo.com");
  EXPECT_FALSE(
      GetContentClientForTesting()
          ->browser()
          ->ShouldTryToUseExistingProcessHost(browser_context(), test_url));
}
#endif

// Tests that RenderProcessHost reuse considers committed sites correctly.
TEST_F(RenderProcessHostUnitTest, ReuseCommittedSite) {
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // BFCache is disabled for this test because the process for |kUrl1| is
  // cached and reused after the navigation to |kUrl2| with BFCache enabled. The
  // test expects that a new process (either spare or created) is used instead.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            site_instance->GetLastProcessAssignmentOutcome());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  NavigateAndCommit(kUrl1);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance->GetLastProcessAssignmentOutcome());

  // Navigate away. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should again return a new process.
  NavigateAndCommit(kUrl2);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_EQ(RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()
                ? SiteInstanceProcessAssignment::USED_SPARE_PROCESS
                : SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            site_instance->GetLastProcessAssignmentOutcome());

  // Now add a subframe that navigates to kUrl1. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy for kUrl1 should now
  // return the process of the subframe RFH.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name, false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->current_frame_host());
  subframe = static_cast<TestRenderFrameHost*>(
      NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, subframe));
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_EQ(subframe->GetProcess(), site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance->GetLastProcessAssignmentOutcome());
}

// Check that only new processes that haven't yet hosted any web content are
// allowed to be reused to host a site requiring a dedicated process.
TEST_F(RenderProcessHostUnitTest, IsUnused) {
  const GURL kUrl1("http://foo.com");

  // A process for a SiteInstance that has no site should be able to host any
  // site that requires a dedicated process.
  EXPECT_FALSE(main_test_rfh()->GetSiteInstance()->HasSite());
  EXPECT_TRUE(main_test_rfh()->GetProcess()->IsUnused());
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context());
    EXPECT_FALSE(site_instance->HasSite());
    EXPECT_TRUE(site_instance->GetProcess()->IsUnused());
  }

  // Navigation should mark the process as unable to become a dedicated process
  // for arbitrary sites.
  NavigateAndCommit(kUrl1);
  EXPECT_FALSE(main_test_rfh()->GetProcess()->IsUnused());

  // A process for a SiteInstance with a preassigned site should be considered
  // "used" from the point the process is created via GetProcess().
  {
    scoped_refptr<SiteInstanceImpl> site_instance = CreateForUrl(kUrl1);
    EXPECT_FALSE(site_instance->GetProcess()->IsUnused());
  }
}

TEST_F(RenderProcessHostUnitTest, ReuseUnmatchedServiceWorkerProcess) {
  const GURL kUrl("https://foo.com");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> site_instance1 = CreateForUrl(kUrl);
  EXPECT_EQ(sw_host2, site_instance1->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host1). sw_host2
  // is no longer unmatched, so sw_host1 is now the newest (and only) process
  // with a corresponding unmatched service worker.
  scoped_refptr<SiteInstanceImpl> site_instance2 = CreateForUrl(kUrl);
  EXPECT_EQ(sw_host1, site_instance2->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation should return a new process
  // because there is no unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> site_instance3 = CreateForUrl(kUrl);
  EXPECT_NE(sw_host1, site_instance3->GetProcess());
  EXPECT_NE(sw_host2, site_instance3->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            site_instance3->GetLastProcessAssignmentOutcome());
}

class UnsuitableHostContentBrowserClient : public ContentBrowserClient {
 public:
  UnsuitableHostContentBrowserClient() {}
  ~UnsuitableHostContentBrowserClient() override {}

 private:
  bool IsSuitableHost(RenderProcessHost* process_host,
                      const GURL& site_url) override {
    return false;
  }
};

// Check that an unmatched ServiceWorker process is not reused when it's not a
// suitable host for the destination URL.  See https://crbug.com/782349.
TEST_F(RenderProcessHostUnitTest,
       DontReuseUnsuitableUnmatchedServiceWorkerProcess) {
  const GURL kUrl("https://foo.com");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host = sw_site_instance->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance->GetLastProcessAssignmentOutcome());

  // Simulate a situation where |sw_host| won't be considered suitable for
  // future navigations to |kUrl|.  In https://crbug.com/782349, this happened
  // when |kUrl| corresponded to a nonexistent extension, but
  // chrome-extension:// URLs can't be tested inside content/.  Instead,
  // install a ContentBrowserClient which will return false when IsSuitableHost
  // is consulted.
  UnsuitableHostContentBrowserClient modified_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Discard the spare, so it cannot be considered by the GetProcess call below.
  SpareRenderProcessHostManagerImpl::Get().CleanupSparesForTesting();

  // Now, getting a RenderProcessHost for a navigation to the same site should
  // not reuse the unmatched service worker's process (i.e., |sw_host|), as
  // it's unsuitable.
  scoped_refptr<SiteInstanceImpl> site_instance = CreateForUrl(kUrl);
  EXPECT_NE(sw_host, site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance->GetLastProcessAssignmentOutcome());

  SetBrowserClientForTesting(regular_client);
}

TEST_F(RenderProcessHostUnitTest, ReuseServiceWorkerProcessForServiceWorker) {
  const GURL kUrl("https://foo.com");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. This is because
  // we use DEFAULT reuse policy for a service worker when we have failed to
  // start the service worker and want to use a new process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of the same site with
  // REUSE_PENDING_OR_COMMITTED_SITE reuse policy should reuse the newest
  // unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_EQ(sw_host2, sw_host3);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance3->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of the same site with
  // REUSE_PENDING_OR_COMMITTED_SITE reuse policy should reuse the newest
  // unmatched service worker's process (i.e., sw_host2). sw_host3 doesn't cause
  // sw_host2 to be considered matched, so we can keep putting more service
  // workers in that process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance4 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host4 = sw_site_instance4->GetProcess();
  EXPECT_EQ(sw_host2, sw_host4);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance4->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host2).
  scoped_refptr<SiteInstanceImpl> site_instance1 = CreateForUrl(kUrl);
  EXPECT_EQ(sw_host2, site_instance1->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site must reuse
  // the newest unmatched service worker's process (i.e., sw_host1). sw_host2
  // is no longer unmatched, so sw_host1 is now the newest (and only) process
  // with a corresponding unmatched service worker.
  scoped_refptr<SiteInstanceImpl> site_instance2 = CreateForUrl(kUrl);
  EXPECT_EQ(sw_host1, site_instance2->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            site_instance2->GetLastProcessAssignmentOutcome());
}

TEST_F(RenderProcessHostUnitTest,
       ReuseServiceWorkerProcessInProcessPerSitePolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  const GURL kUrl("http://foo.com");

  // Gets a RenderProcessHost for a service worker with process-per-site flag.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of the same site with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_EQ(sw_host1, sw_host2);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 = CreateForUrl(kUrl);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_EQ(sw_host1, sw_host3);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance3->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same site again with
  // process-per-site flag should reuse the unmatched service worker's process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance4 = CreateForUrl(kUrl);
  RenderProcessHost* sw_host4 = sw_site_instance4->GetProcess();
  EXPECT_EQ(sw_host1, sw_host4);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance4->GetLastProcessAssignmentOutcome());
}

TEST_F(RenderProcessHostUnitTest, DoNotReuseOtherSiteServiceWorkerProcess) {
  const GURL kUrl1("https://foo.com");
  const GURL kUrl2("https://bar.com");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl1);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of a different site should
  // return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 = CreateForUrl(kUrl2);
  EXPECT_NE(sw_host1, sw_site_instance2->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());
}

class RenderProcessHostWebUIUnitTest : public RenderProcessHostUnitTest {
 public:
  void SetUp() override {
    RenderProcessHostUnitTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableServiceWorkersForChromeScheme);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RenderProcessHostWebUIUnitTest,
       DontReuseServiceWorkerProcessForDifferentWebUI) {
  ScopedWebUIConfigRegistration config_registration1(
      std::make_unique<TestWebUIConfig>("test-host"));
  ScopedWebUIConfigRegistration config_registration2(
      std::make_unique<TestWebUIConfig>("second-host"));

  const GURL kWebUI1("chrome://test-host/");
  const GURL kWebUI2("chrome://second-host/");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kWebUI1);
  RenderProcessHost* sw_host = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting RenderProcessHost for a service worker for a different WebUI
  // should return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 = CreateForUrl(kWebUI2);
  EXPECT_NE(sw_host, sw_site_instance2->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());
}

TEST_F(RenderProcessHostWebUIUnitTest, DontReuseServiceWorkerProcessForWebUrl) {
  ScopedWebUIConfigRegistration config_registration1(
      std::make_unique<TestWebUIConfig>("test-host"));

  const GURL kWebUI1("chrome://test-host/");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kWebUI1);
  RenderProcessHost* sw_host = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  const GURL kWebUrl("https://test.example/");

  // Getting RenderProcessHost for a service worker for a regular site should
  // return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> web_sw_site_instance =
      CreateForServiceWorker(kWebUrl);
  EXPECT_NE(sw_host, web_sw_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            web_sw_site_instance->GetLastProcessAssignmentOutcome());

  // Getting RenderProcessHost for a navigation to a regular site should
  // re-use the Web Service Worker process and not the WebUI one.
  scoped_refptr<SiteInstanceImpl> web_site_instance = CreateForUrl(kWebUrl);
  EXPECT_NE(sw_host, web_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            web_site_instance->GetLastProcessAssignmentOutcome());
}

// Tests that Service Worker processes for WebUIs are not re-used even
// for the same WebUI. Ideally we would re-use the process if it's for
// the same WebUI but we currently don't because of crbug.com/1158277.
TEST_F(RenderProcessHostWebUIUnitTest,
       DontReuseServiceWorkerProcessForSameWebUI) {
  ScopedWebUIConfigRegistration config_registration(
      std::make_unique<TestWebUIConfig>("test-host"));
  const GURL kUrl("chrome://test-host");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. This is because
  // we use DEFAULT reuse policy for a service worker when we have failed to
  // start the service worker and want to use a new process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of the same WebUI with
  // the same WebUI and allow process reuse policy doesn't reuse any service
  // worker processes.
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_NE(sw_host1, sw_host3);
  EXPECT_NE(sw_host2, sw_host3);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance3->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same WebUI doesn't
  // reuse any service worker's processes.
  scoped_refptr<SiteInstanceImpl> site_instance1 = CreateForUrl(kUrl);
  EXPECT_NE(sw_host1, site_instance1->GetProcess());
  EXPECT_NE(sw_host2, site_instance1->GetProcess());
  EXPECT_NE(sw_host3, site_instance1->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to a web URL doesn't reuse any
  // service worker's processes.
  const GURL kWebUrl("https://test.example");
  scoped_refptr<SiteInstanceImpl> web_site_instance = CreateForUrl(kWebUrl);
  EXPECT_NE(sw_host1, web_site_instance->GetProcess());
  EXPECT_NE(sw_host2, web_site_instance->GetProcess());
  EXPECT_NE(sw_host3, web_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            web_site_instance->GetLastProcessAssignmentOutcome());
}

class RenderProcessHostUntrustedWebUIUnitTest
    : public RenderProcessHostUnitTest {
 public:
  void SetUp() override {
    RenderProcessHostUnitTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableServiceWorkersForChromeUntrusted);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(RenderProcessHostUntrustedWebUIUnitTest,
       DontReuseServiceWorkerProcessForDifferentWebUI) {
  ScopedWebUIConfigRegistration config_registration1(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));
  ScopedWebUIConfigRegistration config_registration2(
      std::make_unique<ui::TestUntrustedWebUIConfig>("second-host"));

  const GURL kWebUI1("chrome-untrusted://test-host/");
  const GURL kWebUI2("chrome-untrusted://second-host/");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kWebUI1);
  RenderProcessHost* sw_host = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting RenderProcessHost for a service worker for a different WebUI
  // should return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 = CreateForUrl(kWebUI2);
  EXPECT_NE(sw_host, sw_site_instance2->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());
}

TEST_F(RenderProcessHostUntrustedWebUIUnitTest,
       DontReuseServiceWorkerProcessForWebUrl) {
  ScopedWebUIConfigRegistration config_registration1(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));

  const GURL kWebUI1("chrome-untrusted://test-host/");

  // Gets a RenderProcessHost for an unmatched service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kWebUI1);
  RenderProcessHost* sw_host = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  const GURL kWebUrl("https://test.example/");

  // Getting RenderProcessHost for a service worker for a regular site should
  // return a new process because there is no reusable process.
  scoped_refptr<SiteInstanceImpl> web_sw_site_instance =
      CreateForServiceWorker(kWebUrl);
  EXPECT_NE(sw_host, web_sw_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            web_sw_site_instance->GetLastProcessAssignmentOutcome());

  // Getting RenderProcessHost for a navigation to a regular site should
  // re-use the Web Service Worker process and not the WebUI one.
  scoped_refptr<SiteInstanceImpl> web_site_instance = CreateForUrl(kWebUrl);
  EXPECT_NE(sw_host, web_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            web_site_instance->GetLastProcessAssignmentOutcome());
}

// Tests that Service Worker processes for WebUIs are not re-used even
// for the same WebUI. Ideally we would re-use the process if it's for
// the same WebUI but we currently don't because of crbug.com/1158277.
TEST_F(RenderProcessHostUntrustedWebUIUnitTest,
       DontReuseServiceWorkerProcessForSameWebUI) {
  ScopedWebUIConfigRegistration config_registration(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host"));
  const GURL kUrl("chrome-untrusted://test-host");

  // Gets a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance1 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host1 = sw_site_instance1->GetProcess();
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker with DEFAULT reuse policy
  // should not reuse the existing service worker's process. This is because
  // we use DEFAULT reuse policy for a service worker when we have failed to
  // start the service worker and want to use a new process. We create this
  // second service worker to test the "find the newest process" logic later.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_host2 = sw_site_instance2->GetProcess();
  EXPECT_NE(sw_host1, sw_host2);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a service worker of the same WebUI with
  // REUSE_PENDING_OR_COMMITTED_SITE reuse policy doesn't reuse any service
  // worker processes.
  scoped_refptr<SiteInstanceImpl> sw_site_instance3 =
      CreateForServiceWorker(kUrl,
                             /*can_reuse_process=*/true);
  RenderProcessHost* sw_host3 = sw_site_instance3->GetProcess();
  EXPECT_NE(sw_host1, sw_host3);
  EXPECT_NE(sw_host2, sw_host3);
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            sw_site_instance3->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to the same WebUI doesn't
  // reuse any service worker's processes.
  scoped_refptr<SiteInstanceImpl> site_instance1 = CreateForUrl(kUrl);
  EXPECT_NE(sw_host1, site_instance1->GetProcess());
  EXPECT_NE(sw_host2, site_instance1->GetProcess());
  EXPECT_NE(sw_host3, site_instance1->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            site_instance1->GetLastProcessAssignmentOutcome());

  // Getting a RenderProcessHost for a navigation to a web URL doesn't reuse any
  // service worker's processes.
  const GURL kWebUrl("https://test.example");
  scoped_refptr<SiteInstanceImpl> web_site_instance = CreateForUrl(kWebUrl);
  EXPECT_NE(sw_host1, web_site_instance->GetProcess());
  EXPECT_NE(sw_host2, web_site_instance->GetProcess());
  EXPECT_NE(sw_host3, web_site_instance->GetProcess());
  EXPECT_EQ(SiteInstanceProcessAssignment::CREATED_NEW_PROCESS,
            web_site_instance->GetLastProcessAssignmentOutcome());
}

// Tests that RenderProcessHost will not consider reusing a process that has
// committed an error page.
TEST_F(RenderProcessHostUnitTest, DoNotReuseError) {
  // This test depends on a network error occurring on back navigation.
  // This cannot happen if the page is restored from the back-forward
  // cache, because no network requests would be made.
  contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // Isolate |kUrl1| so we can't get a default SiteInstance for it.
  ChildProcessSecurityPolicyImpl::GetInstance()->AddFutureIsolatedOrigins(
      {url::Origin::Create(kUrl1)},
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST,
      browser_context());

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the main frame navigate to the first url. Getting a RenderProcessHost
  // with the REUSE_PENDING_OR_COMMITTED_SITE policy should now return the
  // process of the main RFH.
  NavigateAndCommit(kUrl1);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Navigate away. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should again return a new process.
  NavigateAndCommit(kUrl2);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Navigate back and simulate an error. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  NavigationSimulator::GoBackAndFail(contents(), net::ERR_TIMED_OUT);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

// Tests that RenderProcessHost reuse considers navigations correctly.
TEST_F(RenderProcessHostUnitTest, ReuseNavigationProcess) {
  const GURL kUrl1("http://foo.com");
  const GURL kUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl1);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl1, main_test_rfh());
  navigation->Start();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl1);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Finish the navigation and start a new cross-site one. Getting
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should
  // return the process of the speculative RenderFrameHost.
  navigation->Commit();
  navigation = NavigationSimulator::CreateBrowserInitiated(kUrl2, contents());
  navigation->Start();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl2);
  EXPECT_EQ(contents()->GetSpeculativePrimaryMainFrame()->GetProcess(),
            site_instance->GetProcess());

  // Remember the process id and cancel the navigation. Getting
  // RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy should
  // no longer return the process of the speculative RenderFrameHost.
  int speculative_process_host_id =
      contents()->GetSpeculativePrimaryMainFrame()->GetProcess()->GetID();
  navigation->Fail(net::ERR_ABORTED);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl2);
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
}

// Tests that RenderProcessHost reuse considers navigations correctly during
// redirects in a renderer-initiated navigation.    Also ensures that with
// --site-per-process, there's no mismatch in origin locks for
// https://crbug.com/773809.
TEST_F(RenderProcessHostUnitTest,
       ReuseNavigationProcessRedirectsRendererInitiated) {
  const GURL kUrl("http://foo.com");
  const GURL kRedirectUrl1("http://foo.com/redirect");
  const GURL kRedirectUrl2("http://bar.com");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(kUrl, main_test_rfh());
  simulator->Start();

  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  // Note that with --site-per-process, the GetProcess() call on
  // |site_instance| will also lock the current process to http://foo.com.
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Simulate a same-site redirect. Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current process.
  simulator->Redirect(kRedirectUrl1);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Simulate a cross-site redirect.  Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy for the initial foo.com site should
  // no longer return the original process.  Getting a RenderProcessHost with
  // the REUSE_PENDING_OR_COMMITTED_SITE policy for the new bar.com site should
  // return the the original process, unless we're in --site-per-process mode.
  simulator->Redirect(kRedirectUrl2);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kRedirectUrl2);
  if (AreAllSitesIsolatedForTesting()) {
    // In --site-per-process, we should've recognized that we will need to swap
    // to a new process; however, the new process won't be created until
    // ready-to-commit time, when the final response comes from bar.com.  Thus,
    // we should have neither foo.com nor bar.com in the original process's
    // list of pending sites, and |site_instance| should create a brand new
    // process that does not match any existing one.  In
    // https://crbug.com/773809, the site_instance->GetProcess() call tried to
    // reuse the original process (already locked to foo.com), leading to
    // origin lock mismatch.
    EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  } else {
    EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  }

  // Once the navigation is ready to commit, getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the current
  // process for the final site, but not the initial one.
  simulator->ReadyToCommit();
  RenderProcessHost* post_redirect_process =
      simulator->GetFinalRenderFrameHost()->GetProcess();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(post_redirect_process, site_instance->GetProcess());
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kRedirectUrl2);
  EXPECT_EQ(post_redirect_process, site_instance->GetProcess());
}

// Tests that RenderProcessHost reuse considers navigations correctly during
// redirects in a browser-initiated navigation.
TEST_F(RenderProcessHostUnitTest,
       ReuseNavigationProcessRedirectsBrowserInitiated) {
  const GURL kInitialUrl("http://google.com");
  const GURL kUrl("http://foo.com");
  const GURL kRedirectUrl1("http://foo.com/redirect");
  const GURL kRedirectUrl2("http://bar.com");

  NavigateAndCommit(kInitialUrl);

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Now getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the speculative
  // process.
  contents()->GetController().LoadURL(kUrl, Referrer(),
                                      ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->SimulateBeforeUnloadCompleted(true);
  int speculative_process_host_id =
      contents()->GetSpeculativePrimaryMainFrame()->GetProcess()->GetID();
  bool speculative_is_default_site_instance =
      contents()
          ->GetSpeculativePrimaryMainFrame()
          ->GetSiteInstance()
          ->IsDefaultSiteInstance();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());

  // Simulate a same-site redirect. Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the speculative
  // process.
  main_test_rfh()->SimulateRedirect(kRedirectUrl1);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());

  // Simulate a cross-site redirect. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should no longer return the
  // speculative process: neither for the new site nor for the initial site we
  // were trying to navigate to. It shouldn't return the current process either.
  main_test_rfh()->SimulateRedirect(kRedirectUrl2);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kRedirectUrl2);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(speculative_is_default_site_instance);
    // The process ID should be the same as the default SiteInstance because
    // kRedirectUrl1 and kRedirectUrl2 do not require a dedicated process.
    EXPECT_EQ(speculative_process_host_id,
              site_instance->GetProcess()->GetID());
  } else {
    EXPECT_NE(speculative_process_host_id,
              site_instance->GetProcess()->GetID());
  }

  // Once the navigation is ready to commit, Getting RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the new speculative
  // process for the final site, but not the initial one. The current process
  // shouldn't be returned either.
  main_test_rfh()->PrepareForCommit();
  speculative_process_host_id =
      contents()->GetSpeculativePrimaryMainFrame()->GetProcess()->GetID();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());
  EXPECT_NE(speculative_process_host_id, site_instance->GetProcess()->GetID());
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kRedirectUrl2);
  EXPECT_EQ(speculative_process_host_id, site_instance->GetProcess()->GetID());
}

// Tests that RenderProcessHost reuse works correctly even if the site URL of a
// URL we're navigating to changes.
TEST_F(RenderProcessHostUnitTest, ReuseExpectedSiteURLChanges) {
  if (AreAllSitesIsolatedForTesting())
    return;

  const GURL kUrl("http://foo.com");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // At first, trying to get a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return a new process.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a navigation. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kUrl, main_test_rfh());
  navigation->Start();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Install the custom ContentBrowserClient. Site URLs are now modified.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as the RFH is
  // registered with the normal site URL.
  EffectiveURLContentBrowserClient modified_client(
      kUrl, kModifiedSiteUrl, /* requires_dedicated_process */ false);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Have the navigation commit. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it was registered with the modified site URL at commit time.
  navigation->Commit();
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Start a reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should return the process of the
  // main RFH.
  contents()->GetController().Reload(ReloadType::NORMAL, false);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Remove the custom ContentBrowserClient. Site URLs are back to normal.
  // Getting a RenderProcessHost with the REUSE_PENDING_OR_COMMITTED_SITE policy
  // should no longer return the process of the main RFH, as it is registered
  // with the modified site URL.
  SetBrowserClientForTesting(regular_client);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_NE(main_test_rfh()->GetProcess(), site_instance->GetProcess());

  // Finish the reload. Getting a RenderProcessHost with the
  // REUSE_PENDING_OR_COMMITTED_SITE policy should now return the process of the
  // main RFH, as it was registered with the regular site URL when it committed.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kUrl);
  site_instance = SiteInstanceImpl::CreateReusableInstanceForTesting(
      browser_context(), kUrl);
  EXPECT_EQ(main_test_rfh()->GetProcess(), site_instance->GetProcess());
}

// Helper test class to modify the StoragePartition returned for a particular
// site URL.
class StoragePartitionContentBrowserClient : public ContentBrowserClient {
 public:
  StoragePartitionContentBrowserClient(const GURL& site,
                                       const std::string& partition_domain,
                                       const std::string& partition_name)
      : site_(site),
        partition_domain_(partition_domain),
        partition_name_(partition_name) {}
  ~StoragePartitionContentBrowserClient() override {}

 private:
  StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site) override {
    if (site == site_) {
      return StoragePartitionConfig::Create(browser_context, partition_domain_,
                                            partition_name_,
                                            false /* in_memory */);
    }

    return StoragePartitionConfig::CreateDefault(browser_context);
  }

  GURL site_;
  std::string partition_domain_;
  std::string partition_name_;
};

// Check that a SiteInstance cannot reuse a RenderProcessHost in a different
// StoragePartition.
TEST_F(RenderProcessHostUnitTest,
       DoNotReuseProcessInDifferentStoragePartition) {
  const GURL kUrl("https://foo.com");
  NavigateAndCommit(kUrl);

  // Change foo.com SiteInstances to use a different StoragePartition.
  StoragePartitionContentBrowserClient modified_client(kUrl, "foo_domain",
                                                       "foo_name");
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Create a foo.com SiteInstance and check that its process does not
  // reuse the foo process from the first navigation, since it's now in a
  // different StoragePartition.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  RenderProcessHost* process = site_instance->GetProcess();
  EXPECT_NE(main_test_rfh()->GetProcess(), process);
  EXPECT_NE(main_test_rfh()->GetProcess()->GetStoragePartition(),
            process->GetStoragePartition());

  // Commit a navigation to foo.com in a new WebContents, so that there is a
  // reusable foo.com process in the new StoragePartition.
  std::unique_ptr<WebContents> contents(CreateTestWebContents());
  static_cast<TestWebContents*>(contents.get())->NavigateAndCommit(kUrl);
  RenderProcessHost* foo_process_in_new_partition =
      contents->GetPrimaryMainFrame()->GetProcess();

  // Create another reusable foo.com SiteInstance in the new StoragePartition,
  // and ensure that this SiteInstance reuse the process just created in that
  // same StoragePartition.
  scoped_refptr<SiteInstanceImpl> site_instance2 =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  RenderProcessHost* process2 = site_instance2->GetProcess();
  EXPECT_EQ(foo_process_in_new_partition, process2);
  EXPECT_EQ(foo_process_in_new_partition->GetStoragePartition(),
            process2->GetStoragePartition());
  EXPECT_NE(main_test_rfh()->GetProcess(), process2);
  EXPECT_NE(main_test_rfh()->GetProcess()->GetStoragePartition(),
            process2->GetStoragePartition());

  SetBrowserClientForTesting(regular_client);
}

// Check that a SiteInstance cannot reuse a ServiceWorker process in a
// different StoragePartition.
TEST_F(RenderProcessHostUnitTest,
       DoNotReuseServiceWorkerProcessInDifferentStoragePartition) {
  const GURL kUrl("https://foo.com");

  // Create a RenderProcessHost for a service worker.
  scoped_refptr<SiteInstanceImpl> sw_site_instance =
      CreateForServiceWorker(kUrl);
  RenderProcessHost* sw_process = sw_site_instance->GetProcess();

  // Change foo.com SiteInstances to use a different StoragePartition.
  StoragePartitionContentBrowserClient modified_client(kUrl, "foo_domain",
                                                       "foo_name");
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Create a foo.com SiteInstance and check that its process does not reuse
  // the ServiceWorker foo.com process, since it's now in a different
  // StoragePartition.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateReusableInstanceForTesting(browser_context(),
                                                         kUrl);
  RenderProcessHost* process = site_instance->GetProcess();
  EXPECT_NE(sw_process, process);

  // Commit a navigation to foo.com in a new WebContents, so that there is a
  // reusable foo.com process in the new StoragePartition.
  std::unique_ptr<WebContents> contents(CreateTestWebContents());
  static_cast<TestWebContents*>(contents.get())->NavigateAndCommit(kUrl);
  RenderProcessHost* foo_process_in_new_partition =
      contents->GetPrimaryMainFrame()->GetProcess();

  // Create a second foo.com service worker, this time in the new
  // StoragePartition. Ensure that it reuses the process registered for foo.com
  // in that same StoragePartition.
  scoped_refptr<SiteInstanceImpl> sw_site_instance2 =
      SiteInstanceImpl::CreateForServiceWorker(
          browser_context(),
          UrlInfo::CreateForTesting(kUrl,
                                    site_instance->GetStoragePartitionConfig()),
          /*can_reuse_process=*/true);
  RenderProcessHost* sw_process2 = sw_site_instance2->GetProcess();
  EXPECT_EQ(sw_process2, foo_process_in_new_partition);
  EXPECT_NE(sw_process2, sw_process);
  EXPECT_EQ(SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS,
            sw_site_instance2->GetLastProcessAssignmentOutcome());

  SetBrowserClientForTesting(regular_client);
}

// Check whether we notify the renderer that it has been locked to a site or
// not. This should depend on the URL from the SiteInstance.
TEST_F(RenderProcessHostUnitTest, RendererLockedToSite) {
  struct TestData {
    GURL test_url;
    bool should_lock_renderer;
  } tests[] = {{GURL("http://"), false},
               {GURL("http://foo.com"), true},
               {GURL("http://bar.foo.com"), true},
               {GURL(""), false},
               {GURL("google.com"), false},
               {GURL("http:"), false},
               {GURL("http://user:pass@google.com:99/foo;bar?q=a#ref"), true}};
  for (const auto& test : tests) {
    scoped_refptr<SiteInstanceImpl> site_instance = CreateForUrl(test.test_url);
    auto* host =
        static_cast<MockRenderProcessHost*>(site_instance->GetProcess());
    if (AreAllSitesIsolatedForTesting())
      EXPECT_EQ(test.should_lock_renderer, host->is_renderer_locked_to_site());
    else
      EXPECT_EQ(false, host->is_renderer_locked_to_site());
  }
}

// Checks that SiteInstanceProcessAssignment::UNKNOWN is used as the zero-value
// when no renderer process has been assigned to the SiteInstance yet.
TEST_F(RenderProcessHostUnitTest, ProcessAssignmentDefault) {
  const GURL kUrl("https://foo.com");

  scoped_refptr<SiteInstanceImpl> site_instance = CreateForUrl(kUrl);
  EXPECT_EQ(SiteInstanceProcessAssignment::UNKNOWN,
            site_instance->GetLastProcessAssignmentOutcome());
  EXPECT_FALSE(site_instance->HasProcess());
}

class SpareRenderProcessHostUnitTest : public RenderViewHostImplTestHarness {
 public:
  SpareRenderProcessHostUnitTest() {}

  SpareRenderProcessHostUnitTest(const SpareRenderProcessHostUnitTest&) =
      delete;
  SpareRenderProcessHostUnitTest& operator=(
      const SpareRenderProcessHostUnitTest&) = delete;

  ~SpareRenderProcessHostUnitTest() override = default;

 protected:
  void SetUp() override {
    SetRenderProcessHostFactory(&rph_factory_);
    RenderViewHostImplTestHarness::SetUp();
    SetContents(nullptr);  // Start with no renderers.
    SpareRenderProcessHostManagerImpl::Get().CleanupSparesForTesting();
    while (!rph_factory_.GetProcesses()->empty()) {
      rph_factory_.Remove(rph_factory_.GetProcesses()->back().get());
    }
  }

  void TearDown() override {
    // Important: Reset the max renderer count to leave this process in a
    // pristine state.
    DeleteContents();
    rph_factory_.GetProcesses()->clear();
    RenderProcessHost::SetMaxRendererProcessCount(0);
    RenderViewHostImplTestHarness::TearDown();
  }

  void PruneDeadRenderProcessHosts() {
    std::list<MockRenderProcessHost*> to_remove;
    for (auto& host : *rph_factory_.GetProcesses()) {
      if (!host->IsInitializedAndNotDead()) {
        to_remove.push_back(host.get());
      }
    }
    for (auto* host : to_remove) {
      rph_factory_.Remove(host);
    }
  }

  MockRenderProcessHostFactory rph_factory_;
};

using SpareProcessMaybeTakeAction =
    RenderProcessHostImpl::SpareProcessMaybeTakeAction;
void ExpectSpareProcessMaybeTakeActionBucket(
    const base::HistogramTester& histograms,
    SpareProcessMaybeTakeAction expected_action) {
  EXPECT_THAT(
      histograms.GetAllSamples(
          "BrowserRenderProcessHost.SpareProcessMaybeTakeAction"),
      testing::ElementsAre(base::Bucket(static_cast<int>(expected_action), 1)));
}

using SpareProcessRefusedByEmbedderReason =
    content::ContentBrowserClient::SpareProcessRefusedByEmbedderReason;
void ExpectSpareProcessRefusedByEmbedderReason(
    const base::HistogramTester& histograms,
    SpareProcessRefusedByEmbedderReason reason) {
  EXPECT_THAT(
      histograms.GetAllSamples(
          "BrowserRenderProcessHost.SpareProcessRefusedByEmbedderReason"),
      testing::ElementsAre(base::Bucket(static_cast<int>(reason), 1)));
}

TEST_F(SpareRenderProcessHostUnitTest, TestRendererTaken) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  spare_manager.WarmupSpare(browser_context());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  ASSERT_EQ(1U, spare_manager.GetSpares().size());
  RenderProcessHost* spare_rph = spare_manager.GetSpares()[0];
  EXPECT_EQ(spare_rph, rph_factory_.GetProcesses()->at(0).get());

  const GURL kUrl1("http://foo.com");
  base::HistogramTester histograms;
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);
  EXPECT_EQ(spare_rph, main_test_rfh()->GetProcess());
  ExpectSpareProcessMaybeTakeActionBucket(
      histograms, SpareProcessMaybeTakeAction::kSpareTaken);

  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    ASSERT_EQ(1U, spare_manager.GetSpares().size());
    EXPECT_NE(spare_rph, spare_manager.GetSpares()[0]);
    EXPECT_EQ(2U, rph_factory_.GetProcesses()->size());
  } else {
    EXPECT_EQ(0U, spare_manager.GetSpares().size());
    EXPECT_EQ(1U, rph_factory_.GetProcesses()->size());
  }
}

TEST_F(SpareRenderProcessHostUnitTest, TestRendererNotTaken) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  std::unique_ptr<BrowserContext> alternate_context(new TestBrowserContext());
  spare_manager.WarmupSpare(alternate_context.get());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  ASSERT_EQ(1U, spare_manager.GetSpares().size());
  RenderProcessHost* old_spare = spare_manager.GetSpares()[0];
  EXPECT_EQ(alternate_context.get(), old_spare->GetBrowserContext());
  EXPECT_EQ(old_spare, rph_factory_.GetProcesses()->at(0).get());
  // Remember the ID of the spare, so as to not compare a pointer of a deleted
  // RenderProcessHost at the end of the test.
  int old_spare_id = old_spare->GetID();

  const GURL kUrl1("http://foo.com");
  base::HistogramTester histograms;
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);
  EXPECT_NE(old_spare_id, main_test_rfh()->GetProcess()->GetID());
  ExpectSpareProcessMaybeTakeActionBucket(
      histograms, SpareProcessMaybeTakeAction::kMismatchedBrowserContext);

  // Pumping the message loop here accounts for the delay between calling
  // RPH::Cleanup on the spare and the time when the posted delete actually
  // happens.  Without pumping, the spare would still be present in
  // rph_factory_.GetProcesses().
  base::RunLoop().RunUntilIdle();
  PruneDeadRenderProcessHosts();

  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    EXPECT_EQ(2U, rph_factory_.GetProcesses()->size());
    ASSERT_EQ(1U, spare_manager.GetSpares().size());
    RenderProcessHost* new_spare = spare_manager.GetSpares()[0];
    ASSERT_NE(old_spare_id, new_spare->GetID());
    EXPECT_EQ(GetBrowserContext(), new_spare->GetBrowserContext());
  } else {
    EXPECT_EQ(1U, rph_factory_.GetProcesses()->size());
    EXPECT_EQ(0U, spare_manager.GetSpares().size());
  }
}

// A mock ContentBrowserClient that returns the
// SpareProcessRefusedByEmbedderReason as set by the user.
class SpareProcessRejectBrowserClient : public ContentBrowserClient {
 public:
  SpareProcessRejectBrowserClient() = default;

  SpareProcessRejectBrowserClient(const SpareProcessRejectBrowserClient&) =
      delete;
  SpareProcessRejectBrowserClient& operator=(
      const SpareProcessRejectBrowserClient&) = delete;

  void SetSpareProcessRefuseReason(SpareProcessRefusedByEmbedderReason reason) {
    refuse_reason_ = reason;
  }

  std::optional<SpareProcessRefusedByEmbedderReason>
  ShouldUseSpareRenderProcessHost(BrowserContext* browser_context,
                                  const GURL& site_url) override {
    return refuse_reason_;
  }

 private:
  SpareProcessRefusedByEmbedderReason refuse_reason_ =
      SpareProcessRefusedByEmbedderReason::DefaultDisabled;
};

TEST_F(SpareRenderProcessHostUnitTest,
       TestSpareProcessRefusedByEmbedderReason) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  spare_manager.WarmupSpare(browser_context());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  ASSERT_EQ(1U, spare_manager.GetSpares().size());
  RenderProcessHost* spare_rph = spare_manager.GetSpares()[0];
  EXPECT_EQ(spare_rph, rph_factory_.GetProcesses()->at(0).get());
  std::vector<SpareProcessRefusedByEmbedderReason> test_reasons = {
      SpareProcessRefusedByEmbedderReason::DefaultDisabled,
      SpareProcessRefusedByEmbedderReason::NoProfile,
      SpareProcessRefusedByEmbedderReason::TopFrameChromeWebUI,
      SpareProcessRefusedByEmbedderReason::InstantRendererForNewTabPage,
      SpareProcessRefusedByEmbedderReason::ExtensionProcess,
      SpareProcessRefusedByEmbedderReason::JitDisabled,
  };
  SpareProcessRejectBrowserClient test_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&test_client);

  for (auto reason : test_reasons) {
    base::HistogramTester histograms;
    test_client.SetSpareProcessRefuseReason(reason);
    SiteInstanceImpl::Create(GetBrowserContext())->GetProcess();
    ExpectSpareProcessRefusedByEmbedderReason(histograms, reason);
    PruneDeadRenderProcessHosts();
    spare_manager.WarmupSpare(browser_context());
  }
  SetBrowserClientForTesting(regular_client);
}

TEST_F(SpareRenderProcessHostUnitTest, SpareMissing) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  spare_manager.CleanupSparesForTesting();
  ASSERT_EQ(0U, rph_factory_.GetProcesses()->size());
  ASSERT_EQ(0U, spare_manager.GetSpares().size());

  const GURL kUrl1("http://foo.com");
  base::HistogramTester histograms;
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);
  EXPECT_TRUE(main_test_rfh()->GetProcess());
  ExpectSpareProcessMaybeTakeActionBucket(
      histograms, SpareProcessMaybeTakeAction::kNoSparePresent);

  if (RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()) {
    EXPECT_EQ(2U, rph_factory_.GetProcesses()->size());
    EXPECT_EQ(1U, spare_manager.GetSpares().size());
  } else {
    EXPECT_EQ(1U, rph_factory_.GetProcesses()->size());
    EXPECT_EQ(0U, spare_manager.GetSpares().size());
  }
}

TEST_F(SpareRenderProcessHostUnitTest,
       SpareShouldNotLaunchInParallelWithOtherProcess) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  std::unique_ptr<BrowserContext> alternate_context(new TestBrowserContext());
  spare_manager.WarmupSpare(alternate_context.get());
  ASSERT_EQ(1U, rph_factory_.GetProcesses()->size());
  ASSERT_EQ(1U, spare_manager.GetSpares().size());
  RenderProcessHost* old_spare = spare_manager.GetSpares()[0];
  EXPECT_EQ(alternate_context.get(), old_spare->GetBrowserContext());
  EXPECT_EQ(old_spare, rph_factory_.GetProcesses()->at(0).get());

  // When we try to get a process for foo.com, we won't be able to use the spare
  // (because it is associated with the alternate, mismatched BrowserContext)
  // and therefore we will have to spawn a new process.  This test verifies that
  // we don't at the same time try to warm-up a new spare (leading to
  // unnecessary resource contention when 2 processes try to launch at the same
  // time).
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForTesting(browser_context(),
                                         GURL("http://foo.com"));
  RenderProcessHost* site_instance_process = site_instance->GetProcess();

  // The SiteInstance shouldn't get the old spare, because of BrowserContext
  // mismatch.  The SiteInstance will get a new process instead.
  EXPECT_NE(old_spare, site_instance_process);
  EXPECT_FALSE(site_instance_process->IsReady());

  // There should be no new spare at this point to avoid launching 2 processes
  // at the same time.  Note that the spare might still be created later during
  // a navigation (e.g. after cross-site redirects or when committing).
  if (!spare_manager.GetSpares().empty()) {
    EXPECT_EQ(old_spare, spare_manager.GetSpares()[0]);
  }
}

// This unit test looks at the simplified equivalent of what
// CtrlClickShouldEndUpInSameProcessTest.BlankTarget test would have
// encountered.  The test verifies that the spare RPH is not launched if 1) we
// need to create another renderer process anyway (e.g. because the spare is
// missing when MaybeTakeSpareRenderProcessHost is called) and 2) creating the
// other renderer process will put as at the process limit.  Launching the spare
// in this scenario would put us over the process limit and is therefore
// undesirable.
TEST_F(SpareRenderProcessHostUnitTest, JustBelowProcessLimit) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  // Override a global. TearDown() will call SetMaxRendererProcessCount() again
  // to clean up.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // No spare or any other renderer process at the start of the test.
  EXPECT_EQ(0U, rph_factory_.GetProcesses()->size());
  EXPECT_TRUE(spare_manager.GetSpares().empty());

  // Navigation can't take a spare (none present) and needs to launch a new
  // renderer process.
  const GURL kUrl1("http://foo.com");
  SetContents(CreateTestWebContents());
  NavigateAndCommit(kUrl1);

  // We should still be below the process limit.
  EXPECT_EQ(1U, rph_factory_.GetProcesses()->size());

  // There should be no spare - having one would put us over the process limit.
  EXPECT_TRUE(spare_manager.GetSpares().empty());
}

// This unit test verifies that a mismatched spare RenderProcessHost is dropped
// before considering process reuse due to the process limit.
TEST_F(SpareRenderProcessHostUnitTest, AtProcessLimit) {
  auto& spare_manager = SpareRenderProcessHostManagerImpl::Get();
  // Override a global. TearDown() will call SetMaxRendererProcessCount() again
  // to clean up.
  RenderProcessHost::SetMaxRendererProcessCount(2);

  // Create and navigate the 1st WebContents.
  const GURL kUrl1("http://foo.com");
  std::unique_ptr<WebContents> contents1(CreateTestWebContents());
  static_cast<TestWebContents*>(contents1.get())->NavigateAndCommit(kUrl1);
  if (!spare_manager.GetSpares().empty()) {
    EXPECT_NE(spare_manager.GetSpares()[0],
              contents1->GetPrimaryMainFrame()->GetProcess());
  }

  // Warm up a mismatched spare.
  std::unique_ptr<BrowserContext> alternate_context(new TestBrowserContext());
  spare_manager.WarmupSpare(alternate_context.get());
  base::RunLoop().RunUntilIdle();
  PruneDeadRenderProcessHosts();
  EXPECT_EQ(2U, rph_factory_.GetProcesses()->size());

  // Create and navigate the 2nd WebContents.
  const GURL kUrl2("http://bar.com");
  std::unique_ptr<WebContents> contents2(CreateTestWebContents());
  static_cast<TestWebContents*>(contents2.get())->NavigateAndCommit(kUrl2);
  base::RunLoop().RunUntilIdle();

  PruneDeadRenderProcessHosts();
  // Creating a 2nd WebContents shouldn't share a renderer process with the 1st
  // one - instead the spare should be dropped to stay under the process limit.
  EXPECT_EQ(2U, rph_factory_.GetProcesses()->size());
  EXPECT_TRUE(spare_manager.GetSpares().empty());
  EXPECT_NE(contents1->GetPrimaryMainFrame()->GetProcess(),
            contents2->GetPrimaryMainFrame()->GetProcess());
}

}  // namespace content
