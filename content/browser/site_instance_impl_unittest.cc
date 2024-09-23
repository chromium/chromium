// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/url_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/storage_partition_test_helpers.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {
namespace {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

bool DoesURLRequireDedicatedProcess(const IsolationContext& isolation_context,
                                    const GURL& url) {
  return SiteInfo::CreateForTesting(isolation_context, url)
      .RequiresDedicatedProcess(isolation_context);
}

SiteInfo CreateSimpleSiteInfo(const GURL& process_lock_url,
                              bool requires_origin_keyed_process) {
  GURL site_url("https://www.foo.com");
  return SiteInfo(site_url, process_lock_url, requires_origin_keyed_process,
                  /*requires_origin_keyed_process_by_default=*/false,
                  /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
                  CreateStoragePartitionConfigForTesting(),
                  WebExposedIsolationInfo::CreateNonIsolated(),
                  WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
                  /*does_site_request_dedicated_process_for_coop=*/false,
                  /*is_jit_disabled=*/false,
                  /*are_v8_optimizations_disabled=*/false, /*is_pdf=*/false,
                  /*is_fenced=*/false,
                  /*agent_cluster_key=*/std::nullopt);
}

}  // namespace

const char kPrivilegedScheme[] = "privileged";
const char kCustomStandardScheme[] = "custom-standard";

class SiteInstanceTestBrowserClient : public TestContentBrowserClient {
 public:
  bool IsSuitableHost(RenderProcessHost* process_host,
                      const GURL& site_url) override {
    return (privileged_process_id_ == process_host->GetID()) ==
        site_url.SchemeIs(kPrivilegedScheme);
  }

  void set_privileged_process_id(int process_id) {
    privileged_process_id_ = process_id;
  }

 private:
  int privileged_process_id_ = -1;
};

class SiteInstanceTest : public testing::Test {
 public:
  SiteInstanceTest() : old_browser_client_(nullptr) {
    url::AddStandardScheme(kPrivilegedScheme, url::SCHEME_WITH_HOST);
    url::AddStandardScheme(kCustomStandardScheme, url::SCHEME_WITH_HOST);
  }

  GURL GetSiteForURL(const IsolationContext& isolation_context,
                     const GURL& url) {
    return SiteInfo::Create(isolation_context, UrlInfo(UrlInfoInit(url)))
        .site_url();
  }

  void SetUp() override {
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &rph_factory_);
    SiteIsolationPolicy::DisableFlagCachingForTesting();

    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    EXPECT_EQ(0U, policy->GetIsolatedOrigins().size())
        << "There should be no isolated origins registered on test startup. "
        << "Some other test probably forgot to clean up the isolated origins "
        << "it added.";
  }

  void TearDown() override {
    // Ensure that no RenderProcessHosts are left over after the tests.
    EXPECT_TRUE(RenderProcessHost::AllHostsIterator().IsAtEnd());

    SetBrowserClientForTesting(old_browser_client_);
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);

    // Many tests in this file register custom isolated origins.  This is
    // stored in global state and could affect behavior in subsequent tests, so
    // ensure that these origins are cleared between test runs.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    policy->ClearIsolatedOriginsForTesting();
  }

  void set_privileged_process_id(int process_id) {
    browser_client_.set_privileged_process_id(process_id);
  }

  void DrainMessageLoop() {
    // We don't just do this in TearDown() because we create TestBrowserContext
    // objects in each test, which will be destructed before
    // TearDown() is called.
    base::RunLoop().RunUntilIdle();
  }

  SiteInstanceTestBrowserClient* browser_client() { return &browser_client_; }

  bool IsIsolatedOrigin(const GURL& url) {
    // It's fine to use an IsolationContext without an associated
    // BrowsingInstance, since this helper is used by tests that deal with
    // globally isolated origins.
    IsolationContext isolation_context(&context_);
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    return policy->IsIsolatedOrigin(isolation_context, url::Origin::Create(url),
                                    false /* origin_requests_isolation */);
  }

  BrowserContext* context() { return &context_; }

  GURL GetSiteForURL(const GURL& url) {
    return GetSiteInfoForURL(url).site_url();
  }

  SiteInfo GetSiteInfoForURL(const std::string& url) {
    return SiteInfo::CreateForTesting(IsolationContext(&context_), GURL(url));
  }

  SiteInfo GetSiteInfoForURL(const GURL& url) {
    return SiteInfo::CreateForTesting(IsolationContext(&context_), url);
  }

  static bool IsSameSite(BrowserContext* context,
                         const GURL& url1,
                         const GURL& url2) {
    return SiteInstanceImpl::IsSameSite(IsolationContext(context),
                                        UrlInfo(UrlInfoInit(url1)),
                                        UrlInfo(UrlInfoInit(url2)),
                                        /*should_compare_effective_urls=*/true);
  }

  // Helper class to watch whether a particular SiteInstance has been
  // destroyed.
  class SiteInstanceDestructionObserver {
   public:
    SiteInstanceDestructionObserver() = default;

    explicit SiteInstanceDestructionObserver(SiteInstanceImpl* site_instance) {
      SetSiteInstance(site_instance);
    }

    void SetSiteInstance(SiteInstanceImpl* site_instance) {
      site_instance_ = site_instance;
      site_instance_->set_destruction_callback_for_testing(
          base::BindOnce(&SiteInstanceDestructionObserver::SiteInstanceDeleting,
                         weak_factory_.GetWeakPtr()));
    }

    void SiteInstanceDeleting() {
      ASSERT_FALSE(site_instance_deleted_);
      ASSERT_FALSE(browsing_instance_deleted_);

      site_instance_deleted_ = true;
      // Infer deletion of the BrowsingInstance.
      if (site_instance_->browsing_instance_->HasOneRef()) {
        browsing_instance_deleted_ = true;
      }
      site_instance_ = nullptr;
    }

    bool site_instance_deleted() { return site_instance_deleted_; }
    bool browsing_instance_deleted() { return browsing_instance_deleted_; }

   private:
    raw_ptr<SiteInstanceImpl> site_instance_ = nullptr;
    bool site_instance_deleted_ = false;
    bool browsing_instance_deleted_ = false;
    base::WeakPtrFactory<SiteInstanceDestructionObserver> weak_factory_{this};
  };

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext context_;

  SiteInstanceTestBrowserClient browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_;
  MockRenderProcessHostFactory rph_factory_;

  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Tests that SiteInfo works correct as a key for std::map and std::set.
// Test SiteInfos with identical site URLs but various lock URLs, including
// variations of each that are origin keyed ("ok").
TEST_F(SiteInstanceTest, SiteInfoAsContainerKey) {
  auto site_info_1 = CreateSimpleSiteInfo(
      GURL("https://foo.com"), false /* requires_origin_keyed_process */);
  auto site_info_1ok = CreateSimpleSiteInfo(
      GURL("https://foo.com"), true /* requires_origin_keyed_process */);
  auto site_info_2 = CreateSimpleSiteInfo(
      GURL("https://www.foo.com"), false /* requires_origin_keyed_process */);
  auto site_info_2ok = CreateSimpleSiteInfo(
      GURL("https://www.foo.com"), true /* requires_origin_keyed_process */);
  auto site_info_3 = CreateSimpleSiteInfo(
      GURL("https://sub.foo.com"), false /* requires_origin_keyed_process */);
  auto site_info_3ok = CreateSimpleSiteInfo(
      GURL("https://sub.foo.com"), true /* requires_origin_keyed_process */);
  auto site_info_4 =
      CreateSimpleSiteInfo(GURL(), false /* requires_origin_keyed_process */);
  auto site_info_4ok =
      CreateSimpleSiteInfo(GURL(), true /* requires_origin_keyed_process */);

  // Test IsSamePrincipalWith.
  EXPECT_TRUE(site_info_1.IsSamePrincipalWith(site_info_1));
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1ok));
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_2));
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_3));
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_4));
  EXPECT_TRUE(site_info_2.IsSamePrincipalWith(site_info_2));
  EXPECT_FALSE(site_info_2.IsSamePrincipalWith(site_info_2ok));
  EXPECT_FALSE(site_info_2.IsSamePrincipalWith(site_info_3));
  EXPECT_FALSE(site_info_2.IsSamePrincipalWith(site_info_4));
  EXPECT_TRUE(site_info_3.IsSamePrincipalWith(site_info_3));
  EXPECT_FALSE(site_info_3.IsSamePrincipalWith(site_info_3ok));
  EXPECT_FALSE(site_info_3.IsSamePrincipalWith(site_info_4));
  EXPECT_TRUE(site_info_4.IsSamePrincipalWith(site_info_4));
  EXPECT_FALSE(site_info_4.IsSamePrincipalWith(site_info_4ok));

  // Test SiteInfoOperators.
  EXPECT_EQ(site_info_1, site_info_1);
  EXPECT_NE(site_info_1, site_info_2);
  EXPECT_NE(site_info_1, site_info_3);
  EXPECT_NE(site_info_1, site_info_4);
  EXPECT_EQ(site_info_2, site_info_2);
  EXPECT_NE(site_info_2, site_info_3);
  EXPECT_NE(site_info_2, site_info_4);
  EXPECT_EQ(site_info_3, site_info_3);
  EXPECT_NE(site_info_3, site_info_4);
  EXPECT_EQ(site_info_4, site_info_4);

  EXPECT_TRUE(site_info_1 < site_info_3);  // 'f' before 's'/
  EXPECT_TRUE(site_info_3 < site_info_2);  // 's' before 'w'/
  EXPECT_TRUE(site_info_4 < site_info_1);  // Empty string first.

  // Check that SiteInfos with differing values of
  // `does_site_request_dedicated_process_for_coop_` are still considered
  // same-principal.
  auto site_info_1_with_isolation_request =
      SiteInfo(GURL("https://www.foo.com") /* site_url */,
               GURL("https://foo.com") /* process_lock_url */,
               /*requires_origin_keyed_process=*/false,
               /*requires_origin_keyed_process_by_default=*/false,
               /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
               CreateStoragePartitionConfigForTesting(),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/true,
               /*is_jit_disabled=*/false,
               /*are_v8_optimizations_disabled=*/false,
               /*is_pdf=*/false, /*is_fenced=*/false,
               /*agent_cluster_key=*/std::nullopt);
  EXPECT_TRUE(
      site_info_1.IsSamePrincipalWith(site_info_1_with_isolation_request));
  EXPECT_EQ(site_info_1, site_info_1_with_isolation_request);

  // Check that SiteInfos with differing values of `is_jit_disabled` are not
  // considered same-principal.
  auto site_info_1_with_jit_disabled =
      SiteInfo(GURL("https://www.foo.com") /* site_url */,
               GURL("https://foo.com") /* process_lock_url */,
               /*requires_origin_keyed_process=*/false,
               /*requires_origin_keyed_process_by_default=*/false,
               /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
               CreateStoragePartitionConfigForTesting(),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/false,
               /*is_jit_disabled=*/true,
               /*are_v8_optimizations_disabled=*/false,
               /*is_pdf=*/false, /*is_fenced=*/false,
               /*agent_cluster_key=*/std::nullopt);
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1_with_jit_disabled));

  // Check that SiteInfos with differing values of
  // `are_v8_optimizations_disabled` are not considered same-principal.
  auto site_info_1_with_optimizations_disabled =
      SiteInfo(GURL("https://www.foo.com") /* site_url */,
               GURL("https://foo.com") /* process_lock_url */,
               /*requires_origin_keyed_process=*/false,
               /*requires_origin_keyed_process_by_default=*/false,
               /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
               CreateStoragePartitionConfigForTesting(),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/false,
               /*is_jit_disabled=*/false,
               /*are_v8_optimizations_disabled=*/true,
               /*is_pdf=*/false, /*is_fenced=*/false,
               /*agent_cluster_key=*/std::nullopt);
  EXPECT_FALSE(
      site_info_1.IsSamePrincipalWith(site_info_1_with_optimizations_disabled));

  // Check that SiteInfos with differing values of `is_pdf` are not considered
  // same-principal.
  auto site_info_1_with_pdf =
      SiteInfo(GURL("https://www.foo.com") /* site_url */,
               GURL("https://foo.com") /* process_lock_url */,
               /*requires_origin_keyed_process=*/false,
               /*requires_origin_keyed_process_by_default=*/false,
               /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
               CreateStoragePartitionConfigForTesting(),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/false,
               /*is_jit_disabled=*/false,
               /*are_v8_optimizations_disabled=*/false, /*is_pdf=*/true,
               /*is_fenced=*/false, /*agent_cluster_key=*/std::nullopt);
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1_with_pdf));

  auto site_info_1_with_is_fenced =
      SiteInfo(GURL("https://www.foo.com") /* site_url */,
               GURL("https://foo.com") /* process_lock_url */,
               /*requires_origin_keyed_process=*/false,
               /*requires_origin_keyed_process_by_default=*/false,
               /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
               CreateStoragePartitionConfigForTesting(),
               WebExposedIsolationInfo::CreateNonIsolated(),
               WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
               /*does_site_request_dedicated_process_for_coop=*/false,
               /*is_jit_disabled=*/false,
               /*are_v8_optimizations_disabled=*/false, /*is_pdf=*/false,
               /*is_fenced=*/true, /*agent_cluster_key=*/std::nullopt);
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1_with_is_fenced));

  {
    std::map<SiteInfo, int> test_map;
    // Map tests: different lock URLs.
    test_map[site_info_1] = 1;
    test_map[site_info_2] = 2;
    test_map[site_info_4] = 4;

    // Make sure std::map treated the different SiteInfo's as distinct.
    EXPECT_EQ(3u, test_map.size());

    // Test that std::map::find() looks up the correct key.
    auto it1 = test_map.find(site_info_1);
    EXPECT_NE(it1, test_map.end());
    EXPECT_EQ(1, it1->second);

    auto it2 = test_map.find(site_info_2);
    EXPECT_NE(it2, test_map.end());
    EXPECT_EQ(2, it2->second);

    EXPECT_EQ(test_map.end(), test_map.find(site_info_3));

    auto it4 = test_map.find(site_info_4);
    EXPECT_NE(it4, test_map.end());
    EXPECT_EQ(4, it4->second);

    // Check that `site_info_1` and `site_info_1_with_isolation_request`
    // collapse into the same key.
    test_map[site_info_1_with_isolation_request] = 5;
    EXPECT_EQ(3u, test_map.size());
    it1 = test_map.find(site_info_1);
    EXPECT_NE(it1, test_map.end());
    EXPECT_EQ(5, it1->second);
  }

  {
    std::map<SiteInfo, int> test_map;
    // Map tests: different lock URLs and origin keys.

    test_map[site_info_1] = 1;
    test_map[site_info_2] = 2;
    test_map[site_info_4] = 4;
    test_map[site_info_1ok] = 11;
    test_map[site_info_2ok] = 12;
    test_map[site_info_4ok] = 14;

    // Make sure std::map treated the different SiteInfo's as distinct.
    EXPECT_EQ(6u, test_map.size());

    // Test that std::map::find() looks up the correct key with
    // requires_origin_keyed_process == true.
    auto it1 = test_map.find(site_info_1ok);
    EXPECT_NE(it1, test_map.end());
    EXPECT_EQ(11, it1->second);

    auto it2 = test_map.find(site_info_2ok);
    EXPECT_NE(it2, test_map.end());
    EXPECT_EQ(12, it2->second);

    EXPECT_EQ(test_map.end(), test_map.find(site_info_3));
    EXPECT_EQ(test_map.end(), test_map.find(site_info_3ok));

    auto it4 = test_map.find(site_info_4ok);
    EXPECT_NE(it4, test_map.end());
    EXPECT_EQ(14, it4->second);
  }

  {
    std::set<SiteInfo> test_set;

    // Set tests.
    test_set.insert(site_info_1);
    test_set.insert(site_info_2);
    test_set.insert(site_info_4);

    EXPECT_EQ(3u, test_set.size());

    auto itS1 = test_set.find(site_info_1);
    auto itS2 = test_set.find(site_info_2);
    auto itS3 = test_set.find(site_info_3);
    auto itS4 = test_set.find(site_info_4);

    EXPECT_NE(test_set.end(), itS1);
    EXPECT_NE(test_set.end(), itS2);
    EXPECT_EQ(test_set.end(), itS3);
    EXPECT_NE(test_set.end(), itS4);

    EXPECT_EQ(site_info_1, *itS1);
    EXPECT_EQ(site_info_2, *itS2);
    EXPECT_EQ(site_info_4, *itS4);
  }
  {
    std::set<SiteInfo> test_set;

    // Set tests, testing requires_origin_keyed_process.
    test_set.insert(site_info_1);
    test_set.insert(site_info_2);
    test_set.insert(site_info_4);
    test_set.insert(site_info_1ok);
    test_set.insert(site_info_2ok);
    test_set.insert(site_info_4ok);

    EXPECT_EQ(6u, test_set.size());

    auto itS1 = test_set.find(site_info_1ok);
    auto itS2 = test_set.find(site_info_2ok);
    auto itS3 = test_set.find(site_info_3ok);
    auto itS4 = test_set.find(site_info_4ok);

    EXPECT_NE(test_set.end(), itS1);
    EXPECT_NE(test_set.end(), itS2);
    EXPECT_EQ(test_set.end(), itS3);
    EXPECT_NE(test_set.end(), itS4);

    EXPECT_EQ(site_info_1ok, *itS1);
    EXPECT_EQ(site_info_2ok, *itS2);
    EXPECT_EQ(site_info_4ok, *itS4);
  }
}

// Test to ensure no memory leaks for SiteInstance objects.
TEST_F(SiteInstanceTest, SiteInstanceDestructor) {
  TestBrowserContext context;

  // The existence of this object will cause WebContentsImpl to create our
  // test one instead of the real one.
  RenderViewHostTestEnabler rvh_test_enabler;
  const GURL url("test:foo");

  // Ensure that instances are deleted when their NavigationEntries are gone.
  scoped_refptr<SiteInstanceImpl> instance = SiteInstanceImpl::Create(&context);
  SiteInstanceDestructionObserver observer(instance.get());
  EXPECT_FALSE(observer.site_instance_deleted());

  std::unique_ptr<NavigationEntryImpl> e1 =
      std::make_unique<NavigationEntryImpl>(
          instance, url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, std::u16string(),
          ui::PAGE_TRANSITION_LINK, false,
          nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);

  EXPECT_FALSE(observer.site_instance_deleted());
  EXPECT_FALSE(observer.browsing_instance_deleted());

  // Add a second reference
  std::unique_ptr<NavigationEntryImpl> e2 =
      std::make_unique<NavigationEntryImpl>(
          instance, url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, std::u16string(),
          ui::PAGE_TRANSITION_LINK, false,
          nullptr /* blob_url_loader_factory */, false /* is_initial_entry */);

  instance = nullptr;

  EXPECT_FALSE(observer.site_instance_deleted());
  EXPECT_FALSE(observer.browsing_instance_deleted());

  // Now delete both entries and be sure the SiteInstance goes away.
  e1.reset();
  EXPECT_FALSE(observer.site_instance_deleted());
  EXPECT_FALSE(observer.browsing_instance_deleted());
  e2.reset();
  // instance is now deleted
  EXPECT_TRUE(observer.site_instance_deleted());
  EXPECT_TRUE(observer.browsing_instance_deleted());
  // browsing_instance is now deleted

  // Ensure that instances are deleted when their RenderFrameHosts are gone.
  std::unique_ptr<TestBrowserContext> browser_context =
      std::make_unique<TestBrowserContext>();
  SiteInstanceDestructionObserver observer2;
  {
    std::unique_ptr<WebContents> web_contents(
        WebContents::Create(WebContents::CreateParams(
            browser_context.get(),
            SiteInstance::Create(browser_context.get()))));
    observer2.SetSiteInstance(static_cast<SiteInstanceImpl*>(
        web_contents->GetPrimaryMainFrame()->GetSiteInstance()));
    EXPECT_FALSE(observer2.site_instance_deleted());
    EXPECT_FALSE(observer2.browsing_instance_deleted());
  }

  // Make sure that we flush any messages related to the above WebContentsImpl
  // destruction.
  DrainMessageLoop();

  EXPECT_TRUE(observer2.site_instance_deleted());
  EXPECT_TRUE(observer2.browsing_instance_deleted());
  // contents is now deleted, along with instance and browsing_instance
}

// Tests that, when using SiteInfo::CreateForTesting with an IsolationContext
// that has no BrowsingInstance, that origins are still correctly given a
// default origin-keyed process when OriginKeyedProcessByDefault is enabled.
TEST_F(SiteInstanceTest,
       OriginKeyedProcessesByDefault_SiteInfo_CreateForTesting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /* enable */ {features::kOriginKeyedProcessesByDefault},
      /* disable */ {});

  TestBrowserContext browser_context;
  GURL url("https://www.foo.com/");
  SiteInfo site_info =
      SiteInfo::CreateForTesting(IsolationContext(&browser_context), url);
  // Note: for Android we normally expect `ShouldEnableStrictSiteIsolation()` to
  // default to false. But if --site-per-process is enabled, that will override
  // and force UseDedicatedProcessesForAllSites() to become true.
  bool dedicated_processes_for_all_sites =
      SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
  EXPECT_EQ(dedicated_processes_for_all_sites,
            site_info.requires_origin_keyed_process());
  if (dedicated_processes_for_all_sites) {
    EXPECT_EQ(url, site_info.process_lock_url());
  } else {
    EXPECT_EQ(GURL("https://foo.com/"), site_info.process_lock_url());
  }
}

// Verifies some basic properties of default SiteInstances.
TEST_F(SiteInstanceTest, DefaultSiteInstanceProperties) {
  TestBrowserContext browser_context;

  // Make sure feature list command-line options are set in a way that forces
  // default SiteInstance creation on all platforms.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /* enable */ {features::kProcessSharingWithDefaultSiteInstances},
      /* disable */ {features::kProcessSharingWithStrictSiteInstances});
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      features::kProcessSharingWithDefaultSiteInstances));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      features::kProcessSharingWithStrictSiteInstances));

  base::test::ScopedCommandLine scoped_command_line;
  // Disable site isolation so we can get default SiteInstances on all
  // platforms.
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableSiteIsolation);
  // If --site-per-process was manually appended, remove it; this interferes
  // with default SiteInstances.
  scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
      switches::kSitePerProcess);

  auto site_instance = SiteInstanceImpl::CreateForTesting(
      &browser_context, GURL("http://foo.com"));
  EXPECT_TRUE(site_instance->IsDefaultSiteInstance());
  EXPECT_TRUE(site_instance->HasSite());
  EXPECT_EQ(site_instance->GetSiteInfo(),
            SiteInfo::CreateForDefaultSiteInstance(
                site_instance->GetIsolationContext(),
                StoragePartitionConfig::CreateDefault(&browser_context),
                WebExposedIsolationInfo::CreateNonIsolated()));
  EXPECT_FALSE(site_instance->RequiresDedicatedProcess());
}

// Ensure that default SiteInstances are deleted when all references to them
// are gone.
TEST_F(SiteInstanceTest, DefaultSiteInstanceDestruction) {
  TestBrowserContext browser_context;
  base::test::ScopedCommandLine scoped_command_line;

  // Disable site isolation so we can get default SiteInstances on all
  // platforms.
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableSiteIsolation);

  // Ensure that default SiteInstances are deleted when all references to them
  // are gone.
  auto site_instance = SiteInstanceImpl::CreateForTesting(
      &browser_context, GURL("http://foo.com"));
  SiteInstanceDestructionObserver observer(site_instance.get());

  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance->IsDefaultSiteInstance());

  site_instance.reset();

  EXPECT_TRUE(observer.site_instance_deleted());
  EXPECT_TRUE(observer.browsing_instance_deleted());
}

// Test to ensure GetProcess returns and creates processes correctly.
TEST_F(SiteInstanceTest, GetProcess) {
  // Ensure that GetProcess returns a process.
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));
  RenderProcessHost* host1 = instance->GetProcess();
  EXPECT_TRUE(host1 != nullptr);

  // Ensure that GetProcess creates a new process.
  scoped_refptr<SiteInstanceImpl> instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  RenderProcessHost* host2 = instance2->GetProcess();
  EXPECT_TRUE(host2 != nullptr);
  EXPECT_NE(host1, host2);

  DrainMessageLoop();
}

// Test to ensure SetSite and site() work properly.
TEST_F(SiteInstanceTest, SetSite) {
  TestBrowserContext context;

  scoped_refptr<SiteInstanceImpl> instance(SiteInstanceImpl::Create(&context));
  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  instance->SetSite(
      UrlInfo::CreateForTesting(GURL("http://www.google.com/index.html")));
  EXPECT_EQ(GURL("http://google.com"), instance->GetSiteURL());

  EXPECT_TRUE(instance->HasSite());

  DrainMessageLoop();
}

// Test to ensure GetSiteForURL properly returns sites for URLs.
TEST_F(SiteInstanceTest, GetSiteForURL) {
  TestBrowserContext context;

  bool origin_keyed_processes_by_default =
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();

  // Pages are irrelevant.
  GURL test_url = GURL("http://www.google.com/index.html");
  GURL site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://google.com"), site_url);
  EXPECT_EQ("http", site_url.scheme());
  EXPECT_EQ("google.com", site_url.host());

  // Ports are irrelevant.
  test_url = GURL("https://www.google.com:8080");
  site_url = GetSiteForURL(test_url);
  if (origin_keyed_processes_by_default) {
    // Ports *are* included when isolating by origin.
    EXPECT_EQ(test_url, site_url);
  } else {
    EXPECT_EQ(GURL("https://google.com"), site_url);
  }

  // Punycode is canonicalized.
  test_url = GURL("http://☃snowperson☃.net:333/");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://xn--snowperson-di0gka.net"), site_url);

  // Username and password are stripped out.
  test_url = GURL("ftp://username:password@ftp.chromium.org/files/README");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("ftp://chromium.org"), site_url);

  // Literal IP addresses of any flavor are okay.
  test_url = GURL("http://127.0.0.1/a.html");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://127.0.0.1"), site_url);
  EXPECT_EQ("127.0.0.1", site_url.host());

  test_url = GURL("http://2130706433/a.html");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://127.0.0.1"), site_url);
  EXPECT_EQ("127.0.0.1", site_url.host());

  test_url = GURL("http://[::1]:2/page.html");
  site_url = GetSiteForURL(test_url);
  if (origin_keyed_processes_by_default) {
    EXPECT_EQ(GURL("http://[::1]:2"), site_url);
  } else {
    EXPECT_EQ(GURL("http://[::1]"), site_url);
  }
  EXPECT_EQ("[::1]", site_url.host());

  // Hostnames without TLDs are okay.
  test_url = GURL("http://foo/a.html");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://foo"), site_url);
  EXPECT_EQ("foo", site_url.host());

  // File URLs should include the scheme.
  test_url = GURL("file:///C:/Downloads/");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Some file URLs have hosts in the path.  For consistency with Blink (which
  // maps *all* file://... URLs into "file://" origin) such file URLs still need
  // to map into "file:" site URL.  See also https://crbug.com/776160.
  test_url = GURL("file://server/path");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Data URLs should have the scheme and the nonce of their opaque origin.
  test_url = GURL("data:text/html,foo");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ("data", site_url.scheme());

  // Check that there is a serialized nonce in the site URL. The nonce is
  // different each time, but has length 32.
  EXPECT_EQ(32u, site_url.GetContent().length());
  EXPECT_FALSE(site_url.EqualsIgnoringRef(test_url));
  EXPECT_FALSE(site_url.has_host());
  test_url = GURL("data:text/html,foo#bar");
  site_url = GetSiteForURL(test_url);
  EXPECT_FALSE(site_url.has_ref());
  EXPECT_NE(test_url, site_url);

  // Javascript URLs should include the scheme.
  test_url = GURL("javascript:foo();");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("javascript:"), site_url);
  EXPECT_EQ("javascript", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Blob URLs extract the site from the origin.
  test_url = GURL(
      "blob:https://www.ftp.chromium.org/"
      "4d4ff040-6d61-4446-86d3-13ca07ec9ab9");
  site_url = GetSiteForURL(test_url);
  if (origin_keyed_processes_by_default) {
    EXPECT_EQ(GURL("https://www.ftp.chromium.org"), site_url);
  } else {
    EXPECT_EQ(GURL("https://chromium.org"), site_url);
  }

  // Blob URLs with file origin also extract the site from the origin.
  test_url = GURL("blob:file:///1029e5a4-2983-4b90-a585-ed217563acfeb");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Blob URLs created from a unique origin use the full URL as the site URL,
  // except for the hash.
  test_url = GURL("blob:null/1029e5a4-2983-4b90-a585-ed217563acfeb");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(test_url, site_url);
  test_url = GURL("blob:null/1029e5a4-2983-4b90-a585-ed217563acfeb#foo");
  site_url = GetSiteForURL(test_url);
  EXPECT_FALSE(site_url.has_ref());
  EXPECT_NE(test_url, site_url);
  EXPECT_TRUE(site_url.EqualsIgnoringRef(test_url));

  // Private domains are preserved, appspot being such a site.
  test_url = GURL(
      "blob:http://www.example.appspot.com:44/"
      "4d4ff040-6d61-4446-86d3-13ca07ec9ab9");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://example.appspot.com"), site_url);

  // The site of filesystem URLs is determined by the inner URL.
  test_url = GURL("filesystem:http://www.google.com/foo/bar.html?foo#bar");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://google.com"), site_url);

  // Error page URLs.
  auto error_site_info =
      SiteInfo::CreateForErrorPage(CreateStoragePartitionConfigForTesting(),
                                   /*is_guest=*/false, /*is_fenced=*/false,
                                   WebExposedIsolationInfo::CreateNonIsolated(),
                                   WebExposedIsolationLevel::kNotIsolated);
  test_url = GURL(kUnreachableWebDataURL);
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(error_site_info.site_url(), site_url);

  // Verify that other URLs that use the chrome-error scheme also map
  // to the error page SiteInfo. These type of URLs should not appear in the
  // codebase, but the mapping is intended to cover the whole scheme.
  test_url = GURL("chrome-error://someerror");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(error_site_info.site_url(), site_url);

  DrainMessageLoop();
}

// Test that process lock URLs are computed without using effective URLs.
TEST_F(SiteInstanceTest, ProcessLockDoesNotUseEffectiveURL) {
  GURL test_url("https://some.app.foo.com/");
  GURL nonapp_site_url("https://foo.com/");
  GURL app_url("https://app.com/");
  EffectiveURLContentBrowserClient modified_client(
      test_url, app_url, /* requires_dedicated_process */ true);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  IsolationContext isolation_context(browser_context.get());

  // Sanity check that SiteInfo fields influenced by effective URLs are set
  // properly.  The site URL should correspond to the effective URL's site
  // (app.com), and the process lock URL should refer to the original URL's site
  // (foo.com).
  {
    bool origin_keyed_processes_by_default =
        SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();

    auto site_info = SiteInfo::CreateForTesting(isolation_context, test_url);
    if (origin_keyed_processes_by_default) {
      EXPECT_EQ(test_url, site_info.process_lock_url());
    } else {
      EXPECT_EQ(nonapp_site_url, site_info.process_lock_url());
    }
    EXPECT_EQ(app_url, site_info.site_url());
  }

  bool is_origin_keyed_processes_by_default =
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();
  GURL expected_process_lock_url =
      is_origin_keyed_processes_by_default ? test_url : nonapp_site_url;
  SiteInfo expected_site_info(
      app_url /* site_url */, expected_process_lock_url,
      is_origin_keyed_processes_by_default,
      is_origin_keyed_processes_by_default,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false,
      /*agent_cluster_key=*/std::nullopt);

  // New SiteInstance in a new BrowsingInstance with a predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForTesting(browser_context.get(), test_url);
    EXPECT_EQ(expected_site_info, site_instance->GetSiteInfo());
  }

  // New related SiteInstance from an existing SiteInstance with a
  // predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> bar_site_instance =
        SiteInstanceImpl::CreateForTesting(browser_context.get(),
                                           GURL("https://bar.com/"));
    scoped_refptr<SiteInstance> site_instance =
        bar_site_instance->GetRelatedSiteInstance(test_url);
    auto* site_instance_impl =
        static_cast<SiteInstanceImpl*>(site_instance.get());
    EXPECT_EQ(expected_site_info, site_instance_impl->GetSiteInfo());
  }

  // New SiteInstance with a lazily assigned site URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context.get());
    EXPECT_FALSE(site_instance->HasSite());
    site_instance->SetSite(UrlInfo::CreateForTesting(test_url));
    EXPECT_EQ(expected_site_info, site_instance->GetSiteInfo());
  }

  SetBrowserClientForTesting(regular_client);
}

// Test of distinguishing URLs from different sites.  Most of this logic is
// tested in RegistryControlledDomainTest.  This test focuses on URLs with
// different schemes or ports.
TEST_F(SiteInstanceTest, IsSameSite) {
  TestBrowserContext context;
  GURL url_foo = GURL("http://foo/a.html");
  GURL url_foo2 = GURL("http://foo/b.html");
  GURL url_foo_https = GURL("https://foo/a.html");
  GURL url_foo_port = GURL("http://foo:8080/a.html");
  GURL url_javascript = GURL("javascript:alert(1);");
  GURL url_blank = GURL(url::kAboutBlankURL);

  // Same scheme and port -> same site.
  EXPECT_TRUE(IsSameSite(&context, url_foo, url_foo2));

  // Different scheme -> different site.
  EXPECT_FALSE(IsSameSite(&context, url_foo, url_foo_https));

  // Different port -> same site.
  // (Changes to document.domain make renderer ignore the port.)
  EXPECT_TRUE(IsSameSite(&context, url_foo, url_foo_port));

  // JavaScript links should be considered same site for anything.
  EXPECT_TRUE(IsSameSite(&context, url_javascript, url_foo));
  EXPECT_TRUE(IsSameSite(&context, url_javascript, url_foo_https));
  EXPECT_TRUE(IsSameSite(&context, url_javascript, url_foo_port));

  // Navigating to a blank page is considered the same site.
  EXPECT_TRUE(IsSameSite(&context, url_foo, url_blank));
  EXPECT_TRUE(IsSameSite(&context, url_foo_https, url_blank));
  EXPECT_TRUE(IsSameSite(&context, url_foo_port, url_blank));

  // Navigating from a blank site is not considered to be the same site.
  EXPECT_FALSE(IsSameSite(&context, url_blank, url_foo));
  EXPECT_FALSE(IsSameSite(&context, url_blank, url_foo_https));
  EXPECT_FALSE(IsSameSite(&context, url_blank, url_foo_port));

  DrainMessageLoop();
}

// Test that two file URLs are considered same-site if they have the same path,
// even if they have different fragments.
TEST_F(SiteInstanceTest, IsSameSiteForFileURLs) {
  TestBrowserContext context;

  // Two identical file URLs should be same-site.
  EXPECT_TRUE(IsSameSite(&context, GURL("file:///foo/bar.html"),
                         GURL("file:///foo/bar.html")));

  // File URLs with the same path but different fragment are considered
  // same-site.
  EXPECT_TRUE(IsSameSite(&context, GURL("file:///foo/bar.html"),
                         GURL("file:///foo/bar.html#baz")));
  EXPECT_TRUE(IsSameSite(&context, GURL("file:///foo/bar.html#baz"),
                         GURL("file:///foo/bar.html")));
  EXPECT_TRUE(IsSameSite(&context, GURL("file:///foo/bar.html#baz"),
                         GURL("file:///foo/bar.html#qux")));
  EXPECT_TRUE(IsSameSite(&context, GURL("file:///#abc"), GURL("file:///#def")));

  // Other cases are cross-site.
  EXPECT_FALSE(IsSameSite(&context, GURL("file:///foo.html"),
                          GURL("file:///foo/bar.html")));
  EXPECT_FALSE(
      IsSameSite(&context, GURL("file:///#bar"), GURL("file:///foo/#bar")));
}

// Test to ensure that there is only one SiteInstance per site in a given
// BrowsingInstance, when process-per-site is not in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSite) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessPerSite));
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  BrowsingInstance* browsing_instance = new BrowsingInstance(
      browser_context.get(), WebExposedIsolationInfo::CreateNonIsolated(),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false,
      /*coop_related_group=*/nullptr,
      /*common_coop_origin=*/std::nullopt);

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_a1), false));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(

      browsing_instance->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_b1), false));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(
                UrlInfo::CreateForTesting(url_a2), false));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same or different
  // browser context) should return a different SiteInstance.
  BrowsingInstance* browsing_instance2 = new BrowsingInstance(
      browser_context.get(), WebExposedIsolationInfo::CreateNonIsolated(),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false,
      /*coop_related_group=*/nullptr,
      /*common_coop_origin=*/std::nullopt);
  // Ensure the new SiteInstance is ref counted so that it gets deleted.
  scoped_refptr<SiteInstanceImpl> site_instance_a2_2(
      browsing_instance2->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_a2), false));
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_2.get());
  EXPECT_FALSE(
      site_instance_a1->IsRelatedSiteInstance(site_instance_a2_2.get()));

  // The two SiteInstances for http://google.com should not use the same process
  // if process-per-site is not enabled.
  RenderProcessHost* process_a1 = site_instance_a1->GetProcess();
  RenderProcessHost* process_a2_2 = site_instance_a2_2->GetProcess();
  EXPECT_NE(process_a1, process_a2_2);

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GetSiteInfoForURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("http://mail.yahoo.com")));

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("https://www.google.com")));
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GetSiteInfoForURL("http://www.yahoo.com")));

  // browsing_instances will be deleted when their SiteInstances are deleted.
  // The processes will be unregistered when the RPH scoped_ptrs go away.

  DrainMessageLoop();
}

// Test to ensure that there is only one RenderProcessHost per site for an
// entire BrowserContext, if process-per-site is in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSiteInBrowserContext) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_refptr<BrowsingInstance> browsing_instance = new BrowsingInstance(
      browser_context.get(), WebExposedIsolationInfo::CreateNonIsolated(),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false,
      /*coop_related_group=*/nullptr,
      /*common_coop_origin=*/std::nullopt);

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_a1), false));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);
  RenderProcessHost* process_a1 = site_instance_a1->GetProcess();

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(
      browsing_instance->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_b1), false));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(
                UrlInfo::CreateForTesting(url_a2), false));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same browser
  // context) should return a different SiteInstance with the same process.
  BrowsingInstance* browsing_instance2 = new BrowsingInstance(
      browser_context.get(), WebExposedIsolationInfo::CreateNonIsolated(),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false,
      /*coop_related_group=*/nullptr,
      /*common_coop_origin=*/std::nullopt);
  scoped_refptr<SiteInstanceImpl> site_instance_a1_2(
      browsing_instance2->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_a1), false));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);
  EXPECT_NE(site_instance_a1.get(), site_instance_a1_2.get());
  EXPECT_EQ(process_a1, site_instance_a1_2->GetProcess());

  // A visit to the original site in a new BrowsingInstance (different browser
  // context) should return a different SiteInstance with a different process.
  std::unique_ptr<TestBrowserContext> browser_context2(
      new TestBrowserContext());
  BrowsingInstance* browsing_instance3 = new BrowsingInstance(
      browser_context2.get(), WebExposedIsolationInfo::CreateNonIsolated(),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false,
      /*coop_related_group=*/nullptr,
      /*common_coop_origin=*/std::nullopt);
  scoped_refptr<SiteInstanceImpl> site_instance_a2_3(
      browsing_instance3->GetSiteInstanceForURL(
          UrlInfo::CreateForTesting(url_a2), false));
  EXPECT_TRUE(site_instance_a2_3.get() != nullptr);
  RenderProcessHost* process_a2_3 = site_instance_a2_3->GetProcess();
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_3.get());
  EXPECT_NE(process_a1, process_a2_3);

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GetSiteInfoForURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("http://mail.yahoo.com")));  // visited before

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(GetSiteInfoForURL(
      "http://www.yahoo.com")));  // different BI, same browser context
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GetSiteInfoForURL("https://www.google.com")));  // not visited before
  EXPECT_FALSE(browsing_instance3->HasSiteInstance(GetSiteInfoForURL(
      "http://www.yahoo.com")));  // different BI, different context

  // browsing_instances will be deleted when their SiteInstances are deleted.
  // The processes will be unregistered when the RPH scoped_ptrs go away.

  DrainMessageLoop();
}

// Test to ensure that IsSuitableForUrlInfo behaves properly for different types
// of URLs.
TEST_F(SiteInstanceTest, IsSuitableForUrlInfo) {
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  RenderProcessHost* host;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  // Check prior to assigning a site or process to the instance, which is
  // expected to return false to allow the SiteInstance to be used for anything.
  EXPECT_TRUE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://google.com"))));

  instance->SetSite(UrlInfo::CreateForTesting(GURL("http://evernote.com/")));
  EXPECT_TRUE(instance->HasSite());

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host = instance->GetProcess();
  EXPECT_TRUE(host != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_TRUE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://evernote.com"))));
  EXPECT_TRUE(instance->IsSuitableForUrlInfo(UrlInfo::CreateForTesting(
      GURL("javascript:alert(document.location.href);"))));

  EXPECT_FALSE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GetWebUIURL(kChromeUIGpuHost))));

  // Test that WebUI SiteInstances reject normal web URLs.
  const GURL webui_url(GetWebUIURL(kChromeUIGpuHost));
  scoped_refptr<SiteInstanceImpl> webui_instance(
      SiteInstanceImpl::Create(browser_context.get()));
  webui_instance->SetSite(UrlInfo::CreateForTesting(webui_url));
  RenderProcessHost* webui_host = webui_instance->GetProcess();

  // Simulate granting WebUI bindings for the process.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
      webui_host->GetID(), BindingsPolicySet({BindingsPolicyValue::kWebUi}));

  EXPECT_TRUE(webui_instance->HasProcess());
  EXPECT_TRUE(webui_instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(webui_url)));
  EXPECT_FALSE(webui_instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://google.com"))));
  EXPECT_FALSE(webui_instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://gpu"))));

  // WebUI uses process-per-site, so another instance will use the same process
  // even if we haven't called GetProcess yet.  Make sure IsSuitableForUrlInfo
  // doesn't crash (http://crbug.com/137070).
  scoped_refptr<SiteInstanceImpl> webui_instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  webui_instance2->SetSite(UrlInfo::CreateForTesting(webui_url));
  EXPECT_TRUE(webui_instance2->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(webui_url)));
  EXPECT_FALSE(webui_instance2->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://google.com"))));

  DrainMessageLoop();
}

// Test to ensure that IsSuitableForUrlInfo behaves properly even when
// --site-per-process is used (http://crbug.com/160671).
TEST_F(SiteInstanceTest, IsSuitableForUrlInfoInSitePerProcess) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  RenderProcessHost* host;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  // Check prior to assigning a site or process to the instance, which is
  // expected to return false to allow the SiteInstance to be used for anything.
  EXPECT_TRUE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://google.com"))));

  instance->SetSite(UrlInfo::CreateForTesting(GURL("http://evernote.com/")));
  EXPECT_TRUE(instance->HasSite());

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host = instance->GetProcess();
  EXPECT_TRUE(host != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_TRUE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GURL("http://evernote.com"))));
  EXPECT_TRUE(instance->IsSuitableForUrlInfo(UrlInfo::CreateForTesting(
      GURL("javascript:alert(document.location.href);"))));

  EXPECT_FALSE(instance->IsSuitableForUrlInfo(
      UrlInfo::CreateForTesting(GetWebUIURL(kChromeUIGpuHost))));

  DrainMessageLoop();
}

// Test that we do not reuse a process in process-per-site mode if it has the
// wrong bindings for its URL.  http://crbug.com/174059.
TEST_F(SiteInstanceTest, ProcessPerSiteWithWrongBindings) {
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  RenderProcessHost* host;
  RenderProcessHost* host2;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  // Simulate navigating to a WebUI URL in a process that does not have WebUI
  // bindings.  This already requires bypassing security checks.
  const GURL webui_url(GetWebUIURL(kChromeUIGpuHost));
  instance->SetSite(UrlInfo::CreateForTesting(webui_url));
  EXPECT_TRUE(instance->HasSite());

  // The call to GetProcess actually creates a new real process.
  host = instance->GetProcess();
  EXPECT_TRUE(host != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  // Without bindings, this should look like the wrong process.
  EXPECT_FALSE(
      instance->IsSuitableForUrlInfo(UrlInfo::CreateForTesting(webui_url)));

  // WebUI uses process-per-site, so another instance would normally use the
  // same process.  Make sure it doesn't use the same process if the bindings
  // are missing.
  scoped_refptr<SiteInstanceImpl> instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  instance2->SetSite(UrlInfo::CreateForTesting(webui_url));
  host2 = instance2->GetProcess();
  EXPECT_TRUE(host2 != nullptr);
  EXPECT_TRUE(instance2->HasProcess());
  EXPECT_NE(host, host2);

  DrainMessageLoop();
}

// Test that we do not register processes with empty sites for process-per-site
// mode.
TEST_F(SiteInstanceTest, NoProcessPerSiteForEmptySite) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  instance->SetSite(UrlInfo());
  EXPECT_TRUE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());
  instance->GetProcess();

  EXPECT_FALSE(RenderProcessHostImpl::GetSoleProcessHostForSite(
      instance->GetIsolationContext(), SiteInfo(browser_context.get())));

  DrainMessageLoop();
}

// Check that an URL is considered same-site with blob: and filesystem: URLs
// with a matching inner origin.  See https://crbug.com/726370.
TEST_F(SiteInstanceTest, IsSameSiteForNestedURLs) {
  TestBrowserContext context;
  GURL foo_url("http://foo.com/");
  GURL bar_url("http://bar.com/");
  GURL blob_foo_url("blob:http://foo.com/uuid");
  GURL blob_bar_url("blob:http://bar.com/uuid");
  GURL fs_foo_url("filesystem:http://foo.com/path/");
  GURL fs_bar_url("filesystem:http://bar.com/path/");

  EXPECT_TRUE(IsSameSite(&context, foo_url, blob_foo_url));
  EXPECT_TRUE(IsSameSite(&context, blob_foo_url, foo_url));
  EXPECT_FALSE(IsSameSite(&context, foo_url, blob_bar_url));
  EXPECT_FALSE(IsSameSite(&context, blob_foo_url, bar_url));

  EXPECT_TRUE(IsSameSite(&context, foo_url, fs_foo_url));
  EXPECT_TRUE(IsSameSite(&context, fs_foo_url, foo_url));
  EXPECT_FALSE(IsSameSite(&context, foo_url, fs_bar_url));
  EXPECT_FALSE(IsSameSite(&context, fs_foo_url, bar_url));

  EXPECT_TRUE(IsSameSite(&context, blob_foo_url, fs_foo_url));
  EXPECT_FALSE(IsSameSite(&context, blob_foo_url, fs_bar_url));
  EXPECT_FALSE(IsSameSite(&context, blob_foo_url, blob_bar_url));
  EXPECT_FALSE(IsSameSite(&context, fs_foo_url, fs_bar_url));

  // Verify that the scheme and ETLD+1 are used for comparison.
  GURL www_bar_url("http://www.bar.com/");
  GURL bar_org_url("http://bar.org/");
  GURL https_bar_url("https://bar.com/");
  EXPECT_TRUE(IsSameSite(&context, www_bar_url, bar_url));
  EXPECT_TRUE(IsSameSite(&context, www_bar_url, blob_bar_url));
  EXPECT_TRUE(IsSameSite(&context, www_bar_url, fs_bar_url));
  EXPECT_FALSE(IsSameSite(&context, bar_org_url, bar_url));
  EXPECT_FALSE(IsSameSite(&context, bar_org_url, blob_bar_url));
  EXPECT_FALSE(IsSameSite(&context, bar_org_url, fs_bar_url));
  EXPECT_FALSE(IsSameSite(&context, https_bar_url, bar_url));
  EXPECT_FALSE(IsSameSite(&context, https_bar_url, blob_bar_url));
  EXPECT_FALSE(IsSameSite(&context, https_bar_url, fs_bar_url));
}

TEST_F(SiteInstanceTest, StrictOriginIsolation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStrictOriginIsolation);
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kStrictOriginIsolation));

  GURL isolated1_foo_url("http://isolated1.foo.com");
  GURL isolated2_foo_url("http://isolated2.foo.com");
  TestBrowserContext browser_context;
  IsolationContext isolation_context(&browser_context);

  EXPECT_FALSE(IsSameSite(context(), isolated1_foo_url, isolated2_foo_url));
  EXPECT_NE(GetSiteForURL(isolation_context, isolated1_foo_url),
            GetSiteForURL(isolation_context, isolated2_foo_url));

  // A bunch of special cases of origins.
  GURL secure_foo("https://foo.com");
  EXPECT_EQ(GetSiteForURL(isolation_context, secure_foo), secure_foo);
  GURL foo_with_port("http://foo.com:1234");
  EXPECT_EQ(GetSiteForURL(isolation_context, foo_with_port), foo_with_port);
  GURL local_host("http://localhost");
  EXPECT_EQ(GetSiteForURL(isolation_context, local_host), local_host);
  GURL ip_local_host("http://127.0.0.1");
  EXPECT_EQ(GetSiteForURL(isolation_context, ip_local_host), ip_local_host);

  // The following should not get origin-specific SiteInstances, as they don't
  // have valid hosts.
  GURL about_url("about:flags");
  EXPECT_NE(GetSiteForURL(isolation_context, about_url), about_url);

  GURL file_url("file:///home/user/foo");
  EXPECT_NE(GetSiteForURL(isolation_context, file_url), file_url);
}

TEST_F(SiteInstanceTest, IsolatedOrigins) {
  GURL foo_url("http://www.foo.com");
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL isolated_bar_url("http://isolated.bar.com");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_FALSE(IsIsolatedOrigin(isolated_foo_url));
  EXPECT_TRUE(IsSameSite(context(), foo_url, isolated_foo_url));

  policy->AddFutureIsolatedOrigins({url::Origin::Create(isolated_foo_url)},
                                   IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(isolated_foo_url));
  EXPECT_FALSE(IsIsolatedOrigin(foo_url));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://foo.com")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://www.bar.com")));
  EXPECT_TRUE(IsIsolatedOrigin(isolated_foo_url));
  EXPECT_FALSE(IsIsolatedOrigin(foo_url));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://foo.com")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://www.bar.com")));
  // Different scheme.
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://isolated.foo.com")));
  // Different port.
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://isolated.foo.com:12345")));

  policy->AddFutureIsolatedOrigins({url::Origin::Create(isolated_bar_url)},
                                   IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(isolated_bar_url));

  // IsSameSite should compare origins rather than sites if either URL is an
  // isolated origin.
  EXPECT_FALSE(IsSameSite(context(), foo_url, isolated_foo_url));
  EXPECT_FALSE(IsSameSite(context(), isolated_foo_url, foo_url));
  EXPECT_FALSE(IsSameSite(context(), isolated_foo_url, isolated_bar_url));
  EXPECT_TRUE(IsSameSite(context(), isolated_foo_url, isolated_foo_url));

  // Ensure blob and filesystem URLs with isolated origins are compared
  // correctly.
  GURL isolated_blob_foo_url("blob:http://isolated.foo.com/uuid");
  EXPECT_TRUE(IsSameSite(context(), isolated_foo_url, isolated_blob_foo_url));
  GURL isolated_filesystem_foo_url("filesystem:http://isolated.foo.com/bar/");
  EXPECT_TRUE(
      IsSameSite(context(), isolated_foo_url, isolated_filesystem_foo_url));

  // The site URL for an isolated origin should be the full origin rather than
  // eTLD+1.
  IsolationContext isolation_context(context());
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_foo_url));
  EXPECT_EQ(
      isolated_foo_url,
      GetSiteForURL(isolation_context, GURL("http://isolated.foo.com:12345")));
  EXPECT_EQ(isolated_bar_url,
            GetSiteForURL(isolation_context, isolated_bar_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_blob_foo_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_filesystem_foo_url));

  // Isolated origins always require a dedicated process.
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, isolated_foo_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, isolated_bar_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, isolated_blob_foo_url));
  EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context,
                                             isolated_filesystem_foo_url));
}

TEST_F(SiteInstanceTest, IsolatedOriginsWithPort) {
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL isolated_foo_with_port("http://isolated.foo.com:12345");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  {
    base::test::MockLog mock_log;
    EXPECT_CALL(
        mock_log,
        Log(::logging::LOGGING_ERROR, testing::_, testing::_, testing::_,
            ::testing::HasSubstr("Ignoring port number in isolated origin: "
                                 "http://isolated.foo.com:12345")))
        .Times(1);
    mock_log.StartCapturingLogs();

    policy->AddFutureIsolatedOrigins(
        {url::Origin::Create(isolated_foo_with_port)},
        IsolatedOriginSource::TEST);
  }

  EXPECT_TRUE(IsIsolatedOrigin(isolated_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(isolated_foo_with_port));

  IsolationContext isolation_context(context());
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_foo_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_foo_with_port));
}

// Check that only valid isolated origins are allowed to be registered.
TEST_F(SiteInstanceTest, IsValidIsolatedOrigin) {
  // Unique origins are invalid, as are invalid URLs that resolve to
  // unique origins.
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(url::Origin()));
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("invalid.url"))));

  // IP addresses are ok.
  EXPECT_TRUE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://127.0.0.1"))));

  // Hosts without a valid registry-controlled domain are disallowed.  This
  // includes hosts that are themselves a registry-controlled domain.
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://.com/"))));
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://.com./"))));
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://foo/"))));
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://co.uk/"))));
  EXPECT_TRUE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://foo.bar.baz/"))));

  // Scheme must be HTTP or HTTPS.
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GetWebUIURL(kChromeUIGpuHost))));
  EXPECT_TRUE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://a.com"))));
  EXPECT_TRUE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("https://b.co.uk"))));

  // Trailing dot is disallowed.
  EXPECT_FALSE(IsolatedOriginUtil::IsValidIsolatedOrigin(
      url::Origin::Create(GURL("http://a.com."))));
}

TEST_F(SiteInstanceTest, SubdomainOnIsolatedSite) {
  GURL isolated_url("http://isolated.com");
  GURL foo_isolated_url("http://foo.isolated.com");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(isolated_url)},
                                   IsolatedOriginSource::TEST);

  EXPECT_TRUE(IsIsolatedOrigin(isolated_url));
  EXPECT_TRUE(IsIsolatedOrigin(foo_isolated_url));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://unisolated.com")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://isolated.foo.com")));
  // Wrong scheme.
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://foo.isolated.com")));
  // Subdomain with a different port.
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://foo.isolated.com:12345")));

  // Appending a trailing dot to a URL should not bypass process isolation.
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://isolated.com.")));
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://foo.isolated.com.")));

  // A new SiteInstance created for a subdomain on an isolated origin
  // should use the isolated origin's host and not its own host as the site
  // URL.
  IsolationContext isolation_context(context());
  EXPECT_EQ(isolated_url, GetSiteForURL(isolation_context, foo_isolated_url));

  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, foo_isolated_url));

  EXPECT_TRUE(IsSameSite(context(), isolated_url, foo_isolated_url));
  EXPECT_TRUE(IsSameSite(context(), foo_isolated_url, isolated_url));

  // Don't try to match subdomains on IP addresses.
  GURL isolated_ip("http://127.0.0.1");
  policy->AddFutureIsolatedOrigins({url::Origin::Create(isolated_ip)},
                                   IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(isolated_ip));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("http://42.127.0.0.1")));
}

TEST_F(SiteInstanceTest, SubdomainOnIsolatedOrigin) {
  GURL foo_url("http://foo.com");
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL bar_isolated_foo_url("http://bar.isolated.foo.com");
  GURL baz_isolated_foo_url("http://baz.isolated.foo.com");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(isolated_foo_url)},
                                   IsolatedOriginSource::TEST);

  EXPECT_FALSE(IsIsolatedOrigin(foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(isolated_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(bar_isolated_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(baz_isolated_foo_url));

  IsolationContext isolation_context(context());
  EXPECT_EQ(foo_url, GetSiteForURL(isolation_context, foo_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, isolated_foo_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, bar_isolated_foo_url));
  EXPECT_EQ(isolated_foo_url,
            GetSiteForURL(isolation_context, baz_isolated_foo_url));

  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_FALSE(DoesURLRequireDedicatedProcess(isolation_context, foo_url));
  }
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, isolated_foo_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, bar_isolated_foo_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, baz_isolated_foo_url));

  EXPECT_FALSE(IsSameSite(context(), foo_url, isolated_foo_url));
  EXPECT_FALSE(IsSameSite(context(), isolated_foo_url, foo_url));
  EXPECT_FALSE(IsSameSite(context(), foo_url, bar_isolated_foo_url));
  EXPECT_FALSE(IsSameSite(context(), bar_isolated_foo_url, foo_url));
  EXPECT_TRUE(IsSameSite(context(), bar_isolated_foo_url, isolated_foo_url));
  EXPECT_TRUE(IsSameSite(context(), isolated_foo_url, bar_isolated_foo_url));
  EXPECT_TRUE(
      IsSameSite(context(), bar_isolated_foo_url, baz_isolated_foo_url));
  EXPECT_TRUE(
      IsSameSite(context(), baz_isolated_foo_url, bar_isolated_foo_url));
}

TEST_F(SiteInstanceTest, MultipleIsolatedOriginsWithCommonSite) {
  GURL foo_url("http://foo.com");
  GURL bar_foo_url("http://bar.foo.com");
  GURL baz_bar_foo_url("http://baz.bar.foo.com");
  GURL qux_baz_bar_foo_url("http://qux.baz.bar.foo.com");

  IsolationContext isolation_context(context());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(foo_url), url::Origin::Create(baz_bar_foo_url)},
      IsolatedOriginSource::TEST);

  EXPECT_TRUE(IsIsolatedOrigin(foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(bar_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(baz_bar_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(qux_baz_bar_foo_url));

  EXPECT_EQ(foo_url, GetSiteForURL(isolation_context, foo_url));
  EXPECT_EQ(foo_url, GetSiteForURL(isolation_context, bar_foo_url));
  EXPECT_EQ(baz_bar_foo_url, GetSiteForURL(isolation_context, baz_bar_foo_url));
  EXPECT_EQ(baz_bar_foo_url,
            GetSiteForURL(isolation_context, qux_baz_bar_foo_url));

  EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, foo_url));
  EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, bar_foo_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, baz_bar_foo_url));
  EXPECT_TRUE(
      DoesURLRequireDedicatedProcess(isolation_context, qux_baz_bar_foo_url));

  EXPECT_TRUE(IsSameSite(context(), foo_url, bar_foo_url));
  EXPECT_FALSE(IsSameSite(context(), foo_url, baz_bar_foo_url));
  EXPECT_FALSE(IsSameSite(context(), foo_url, qux_baz_bar_foo_url));

  EXPECT_FALSE(IsSameSite(context(), bar_foo_url, baz_bar_foo_url));
  EXPECT_FALSE(IsSameSite(context(), bar_foo_url, qux_baz_bar_foo_url));

  EXPECT_TRUE(IsSameSite(context(), baz_bar_foo_url, qux_baz_bar_foo_url));
}

// Check that new SiteInstances correctly preserve the full URL that was used
// to initialize their site URL.
TEST_F(SiteInstanceTest, OriginalURL) {
  GURL original_url("https://foo.com/");
  GURL app_url("https://app.com/");
  EffectiveURLContentBrowserClient modified_client(
      original_url, app_url, /* requires_dedicated_process */ true);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());

  bool is_origin_keyed_processes_by_default =
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();
  SiteInfo expected_site_info(
      app_url /* site_url */, original_url /* process_lock_url */,
      is_origin_keyed_processes_by_default,
      is_origin_keyed_processes_by_default,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false,
      /*agent_cluster_key=*/std::nullopt);

  // New SiteInstance in a new BrowsingInstance with a predetermined URL.  In
  // this and subsequent cases, the site URL should consist of the effective
  // URL's site, and the process lock URL and original URLs should be based on
  // |original_url|.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForTesting(browser_context.get(), original_url);
    EXPECT_EQ(expected_site_info, site_instance->GetSiteInfo());
    EXPECT_EQ(original_url, site_instance->original_url());
  }

  // New related SiteInstance from an existing SiteInstance with a
  // predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> bar_site_instance =
        SiteInstanceImpl::CreateForTesting(browser_context.get(),
                                           GURL("https://bar.com/"));
    scoped_refptr<SiteInstance> site_instance =
        bar_site_instance->GetRelatedSiteInstance(original_url);
    auto* site_instance_impl =
        static_cast<SiteInstanceImpl*>(site_instance.get());
    EXPECT_EQ(expected_site_info, site_instance_impl->GetSiteInfo());
    EXPECT_EQ(original_url, site_instance_impl->original_url());
  }

  // New SiteInstance with a lazily assigned site URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context.get());
    EXPECT_FALSE(site_instance->HasSite());
    EXPECT_TRUE(site_instance->original_url().is_empty());
    site_instance->SetSite(UrlInfo::CreateForTesting(original_url));
    EXPECT_EQ(expected_site_info, site_instance->GetSiteInfo());
    EXPECT_EQ(original_url, site_instance->original_url());
  }

  SetBrowserClientForTesting(regular_client);
}

TEST_F(SiteInstanceTest, WebExposedIsolationLevel) {
  GURL url("https://example.com/");
  auto origin = url::Origin::Create(url);
  GURL other_url("https://example2.com/");

  // SiteInfos in a non-isolated BrowsingInstance shouldn't be isolated.
  auto non_isolated =
      SiteInfo::Create(IsolationContext(context()),
                       UrlInfo(UrlInfoInit(url).WithWebExposedIsolationInfo(
                           WebExposedIsolationInfo::CreateNonIsolated())));
  EXPECT_FALSE(non_isolated.web_exposed_isolation_info().is_isolated());
  EXPECT_EQ(WebExposedIsolationLevel::kNotIsolated,
            non_isolated.web_exposed_isolation_level());

  // SiteInfos in an isolated BrowsingInstance should be isolated.
  auto isolated_same_origin =
      SiteInfo::Create(IsolationContext(context()),
                       UrlInfo(UrlInfoInit(url).WithWebExposedIsolationInfo(
                           WebExposedIsolationInfo::CreateIsolated(origin))));
  EXPECT_TRUE(isolated_same_origin.web_exposed_isolation_info().is_isolated());
  EXPECT_FALSE(isolated_same_origin.web_exposed_isolation_info()
                   .is_isolated_application());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            isolated_same_origin.web_exposed_isolation_level());

  // Cross-origin SiteInfos in an isolated BrowsingInstance should be isolated.
  auto isolated_cross_origin = SiteInfo::Create(
      IsolationContext(context()),
      UrlInfo(UrlInfoInit(other_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolated(origin))));
  EXPECT_TRUE(isolated_cross_origin.web_exposed_isolation_info().is_isolated());
  EXPECT_FALSE(isolated_cross_origin.web_exposed_isolation_info()
                   .is_isolated_application());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            isolated_cross_origin.web_exposed_isolation_level());

  // Same-origin SiteInfos in an isolated application BrowsingInstance should
  // have the "isolated application" isolation level.
  auto isolated_app_same_origin = SiteInfo::Create(
      IsolationContext(context()),
      UrlInfo(UrlInfoInit(url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolatedApplication(origin))));
  EXPECT_TRUE(
      isolated_app_same_origin.web_exposed_isolation_info().is_isolated());
  EXPECT_TRUE(isolated_app_same_origin.web_exposed_isolation_info()
                  .is_isolated_application());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolatedApplication,
            isolated_app_same_origin.web_exposed_isolation_level());

  // Cross-origin SiteInfos in an isolated application BrowsingInstance should
  // only have the "isolated" isolation level.
  auto isolated_app_cross_origin = SiteInfo::Create(
      IsolationContext(context()),
      UrlInfo(UrlInfoInit(other_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolatedApplication(origin))));
  EXPECT_TRUE(
      isolated_app_cross_origin.web_exposed_isolation_info().is_isolated());
  EXPECT_TRUE(isolated_app_cross_origin.web_exposed_isolation_info()
                  .is_isolated_application());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            isolated_app_cross_origin.web_exposed_isolation_level());

  // Sandboxed iframes should be considered cross-origin and not inherit the
  // application isolation level.
  auto isolated_app_same_origin_sandboxed = SiteInfo::Create(
      IsolationContext(context()),
      UrlInfo(
          UrlInfoInit(url)
              .WithWebExposedIsolationInfo(
                  WebExposedIsolationInfo::CreateIsolatedApplication(origin))
              .WithSandbox(true)));
  EXPECT_TRUE(isolated_app_same_origin_sandboxed.web_exposed_isolation_info()
                  .is_isolated());
  EXPECT_TRUE(isolated_app_same_origin_sandboxed.web_exposed_isolation_info()
                  .is_isolated_application());
  EXPECT_EQ(WebExposedIsolationLevel::kIsolated,
            isolated_app_same_origin_sandboxed.web_exposed_isolation_level());
}

namespace {

ProcessLock ProcessLockFromString(const std::string& url) {
  return ProcessLock::FromSiteInfo(SiteInfo(
      /*site_url=*/GURL(url),
      /*process_lock_url=*/GURL(url),
      /*requires_origin_keyed_process=*/false,
      /*requires_origin_keyed_process_by_default=*/false,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false,
      /*agent_cluster_key=*/std::nullopt));
}

}  // namespace

TEST_F(SiteInstanceTest, IsProcessLockASite) {
  EXPECT_FALSE(ProcessLockFromString("http://").IsASiteOrOrigin());
  EXPECT_FALSE(ProcessLockFromString("").IsASiteOrOrigin());
  EXPECT_FALSE(ProcessLockFromString("google.com").IsASiteOrOrigin());
  EXPECT_FALSE(ProcessLockFromString("http:").IsASiteOrOrigin());
  EXPECT_FALSE(ProcessLockFromString("chrome:").IsASiteOrOrigin());

  EXPECT_TRUE(ProcessLockFromString("http://foo.com").IsASiteOrOrigin());
  EXPECT_TRUE(ProcessLockFromString("http://bar.foo.com").IsASiteOrOrigin());
  EXPECT_TRUE(
      ProcessLockFromString("http://user:pass@google.com:99/foo;bar?q=a#ref")
          .IsASiteOrOrigin());
}

TEST_F(SiteInstanceTest, StartIsolatingSite) {
  // Skip this test case if dynamic isolated origins are not enabled.
  if (!SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return;

  IsolationContext isolation_context(context());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  // StartIsolatingSite() should convert the URL to a site before isolating it.
  SiteInstance::StartIsolatingSite(
      context(), GURL("http://bar.foo.com/foo/html.bar"),
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://foo.com")));
  SiteInstance::StartIsolatingSite(
      context(), GURL("https://a.b.c.com:8000/"),
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(GURL("https://c.com")));
  SiteInstance::StartIsolatingSite(
      context(), GURL("http://bar.com/foo/bar.html"),
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(GURL("http://bar.com")));

  // Attempts to isolate an unsupported isolated origin should be ignored.
  GURL data_url("data:,");
  GURL blank_url(url::kAboutBlankURL);
  SiteInstance::StartIsolatingSite(
      context(), data_url,
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  SiteInstance::StartIsolatingSite(
      context(), blank_url,
      ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
  EXPECT_FALSE(IsIsolatedOrigin(data_url));
  EXPECT_FALSE(IsIsolatedOrigin(blank_url));

  // Cleanup.
  policy->RemoveStateForBrowserContext(*context());
}

TEST_F(SiteInstanceTest, CreateForUrlInfo) {
  class CustomBrowserClient : public EffectiveURLContentBrowserClient {
   public:
    CustomBrowserClient(const GURL& url_to_modify,
                        const GURL& url_to_return,
                        const std::string& empty_scheme)
        : EffectiveURLContentBrowserClient(url_to_modify,
                                           url_to_return,
                                           false) {
      url::AddEmptyDocumentScheme(empty_scheme.c_str());
    }

   private:
    url::ScopedSchemeRegistryForTests scheme_registry_;
  };

  const GURL kNonIsolatedUrl("https://bar.com/");
  const GURL kIsolatedUrl("https://isolated.com/");
  const GURL kFileUrl("file:///C:/Downloads/");
  const GURL kCustomUrl("http://custom.foo.com");
  const GURL kCustomAppUrl(std::string(kCustomStandardScheme) + "://custom");
  const GURL kEmptySchemeUrl("siteless://test");
  CustomBrowserClient modified_client(kCustomUrl, kCustomAppUrl,
                                      kEmptySchemeUrl.scheme());
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  ChildProcessSecurityPolicyImpl::GetInstance()->AddFutureIsolatedOrigins(
      {url::Origin::Create(kIsolatedUrl)}, IsolatedOriginSource::TEST);

  auto instance1 =
      SiteInstanceImpl::CreateForTesting(context(), kNonIsolatedUrl);
  auto instance2 = SiteInstanceImpl::CreateForTesting(context(), kIsolatedUrl);
  auto instance3 = SiteInstanceImpl::CreateForTesting(context(), kFileUrl);
  auto instance4 =
      SiteInstanceImpl::CreateForTesting(context(), GURL(url::kAboutBlankURL));
  auto instance5 = SiteInstanceImpl::CreateForTesting(context(), kCustomUrl);

  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  } else {
    EXPECT_FALSE(instance1->IsDefaultSiteInstance());
    EXPECT_EQ(kNonIsolatedUrl, instance1->GetSiteURL());
  }
  EXPECT_TRUE(instance1->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(kNonIsolatedUrl)));
  EXPECT_TRUE(instance1->IsSameSiteWithURL(kNonIsolatedUrl));

  EXPECT_FALSE(instance2->IsDefaultSiteInstance());
  EXPECT_EQ(kIsolatedUrl, instance2->GetSiteURL());
  EXPECT_TRUE(instance2->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(kIsolatedUrl)));
  EXPECT_TRUE(instance2->IsSameSiteWithURL(kIsolatedUrl));

  EXPECT_FALSE(instance3->IsDefaultSiteInstance());
  EXPECT_EQ(GURL("file:"), instance3->GetSiteURL());
  EXPECT_TRUE(
      instance3->DoesSiteInfoForURLMatch(UrlInfo::CreateForTesting(kFileUrl)));
  // Not same site because file URL's don't have a host.
  EXPECT_FALSE(instance3->IsSameSiteWithURL(kFileUrl));

  // about:blank URLs generate a SiteInstance without the site URL set because
  // ShouldAssignSiteForURL() returns false and the expectation is that the
  // site URL will be set at a later time.
  EXPECT_FALSE(instance4->IsDefaultSiteInstance());
  EXPECT_FALSE(instance4->HasSite());
  EXPECT_FALSE(instance4->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(GURL(url::kAboutBlankURL))));
  EXPECT_FALSE(instance4->IsSameSiteWithURL(GURL(url::kAboutBlankURL)));

  // Test the standard effective URL case.
  EXPECT_TRUE(instance5->HasSite());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance5->IsDefaultSiteInstance());
  } else {
    EXPECT_FALSE(instance5->IsDefaultSiteInstance());
    EXPECT_EQ("custom-standard://custom/", instance5->GetSiteURL());
    EXPECT_EQ("http://foo.com/", instance5->GetSiteInfo().process_lock_url());
  }
  EXPECT_TRUE(instance5->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(kCustomUrl)));
  EXPECT_TRUE(instance5->IsSameSiteWithURL(kCustomUrl));

  // Test the "do not assign site" case.
  if (instance5->IsDefaultSiteInstance()) {
    // Verify that the default SiteInstance is not a site match
    // with |kEmptySchemeUrl| because this URL requires a SiteInstance that
    // does not have its site set.
    EXPECT_FALSE(instance5->DoesSiteInfoForURLMatch(
        UrlInfo::CreateForTesting(kEmptySchemeUrl)));
    EXPECT_FALSE(instance5->IsSameSiteWithURL(kEmptySchemeUrl));
  }

  // Verify that |kEmptySchemeUrl| will always construct a SiteInstance without
  // a site set.
  auto instance6 =
      SiteInstanceImpl::CreateForTesting(context(), kEmptySchemeUrl);
  EXPECT_FALSE(instance6->IsDefaultSiteInstance());
  EXPECT_FALSE(instance6->HasSite());
  EXPECT_FALSE(instance6->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(kEmptySchemeUrl)));
  EXPECT_FALSE(instance6->IsSameSiteWithURL(kEmptySchemeUrl));

  SetBrowserClientForTesting(regular_client);
}

TEST_F(SiteInstanceTest, CreateForGuest) {
  // Verify that a SiteInstance created with CreateForGuest() is considered
  // a <webview> guest and has the correct StoragePartition.
  const StoragePartitionConfig kGuestConfig = StoragePartitionConfig::Create(
      context(), "appid", "partition_name", /*in_memory=*/false);
  auto instance2 = SiteInstanceImpl::CreateForGuest(context(), kGuestConfig);
  EXPECT_TRUE(instance2->IsGuest());
  EXPECT_EQ(instance2->GetStoragePartitionConfig(), kGuestConfig);
}

TEST_F(SiteInstanceTest, DoesSiteRequireDedicatedProcess) {
  // Since this test injects a custom WebUI scheme below, ensure that the
  // list of WebUI schemes isn't cached.  Otherwise, a random unit test running
  // before this test may triggers caching, causing the custom WebUI scheme to
  // never be seen.
  URLDataManagerBackend::SetDisallowWebUISchemeCachingForTesting(true);

  class CustomBrowserClient : public EffectiveURLContentBrowserClient {
   public:
    CustomBrowserClient(const GURL& url_to_modify,
                        const GURL& url_to_return,
                        bool requires_dedicated_process,
                        const std::string& additional_webui_scheme)
        : EffectiveURLContentBrowserClient(url_to_modify,
                                           url_to_return,
                                           requires_dedicated_process),
          additional_webui_scheme_(additional_webui_scheme) {
      DCHECK(!additional_webui_scheme.empty());
    }

   private:
    void GetAdditionalWebUISchemes(
        std::vector<std::string>* additional_schemes) override {
      additional_schemes->push_back(additional_webui_scheme_);
    }

    const std::string additional_webui_scheme_;
  };

  const std::vector<std::string> kUrlsThatDoNotRequireADedicatedProcess = {
      "about:blank",
      "http://foo.com",
      "data:text/html,Hello World!",
      "file:///tmp/test.txt",
  };

  const char* kExplicitlyIsolatedURL = "http://isolated.com";
  const char* kCustomWebUIScheme = "my-webui";
  const char* kCustomWebUIUrl = "my-webui://show-stats";
  const char* kCustomUrl = "http://custom.foo.com";
  const char* kCustomAppUrl = "custom-scheme://custom";
  const std::vector<std::string> kUrlsThatAlwaysRequireADedicatedProcess = {
      kExplicitlyIsolatedURL,
      kUnreachableWebDataURL,
      GetWebUIURLString("network-error"),
      kCustomUrl,
      kCustomAppUrl,
      kCustomWebUIUrl,
  };

  CustomBrowserClient modified_client(GURL(kCustomUrl), GURL(kCustomAppUrl),
                                      /* requires_dedicated_process */ true,
                                      kCustomWebUIScheme);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  IsolationContext isolation_context(context());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins(
      {url::Origin::Create(GURL(kExplicitlyIsolatedURL))},
      IsolatedOriginSource::TEST);

  for (const auto& url : kUrlsThatAlwaysRequireADedicatedProcess) {
    EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, GURL(url)))
        << " failing url: " << url;
  }

  for (const auto& url : kUrlsThatDoNotRequireADedicatedProcess) {
    EXPECT_EQ(AreAllSitesIsolatedForTesting(),
              DoesURLRequireDedicatedProcess(isolation_context, GURL(url)))
        << " failing url: " << url;
  }
  SetBrowserClientForTesting(regular_client);
  URLDataManagerBackend::SetDisallowWebUISchemeCachingForTesting(false);
}

TEST_F(SiteInstanceTest, DoWebUIURLsWithSubdomainsUseTLDForProcessLock) {
  class CustomWebUIWebUIControllerFactory : public WebUIControllerFactory {
   public:
    std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
        WebUI* web_ui,
        const GURL& url) override {
      return nullptr;
    }
    WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                               const GURL& url) override {
      return WebUI::kNoWebUI;
    }
    bool UseWebUIForURL(BrowserContext* browser_context,
                        const GURL& url) override {
      return HasWebUIScheme(url);
    }
  };
  CustomWebUIWebUIControllerFactory factory;
  content::ScopedWebUIControllerFactoryRegistration factory_registration(
      &factory);

  const GURL webui_tld_url = GetWebUIURL("foo");
  const GURL webui_host_bar_url = GetWebUIURL("bar.foo");
  const GURL webui_host_baz_url = GetWebUIURL("baz.foo");

  const SiteInfo webui_tld_site_info = GetSiteInfoForURL(webui_tld_url);
  const SiteInfo webui_host_bar_site_info =
      GetSiteInfoForURL(webui_host_bar_url);
  const SiteInfo webui_host_baz_site_info =
      GetSiteInfoForURL(webui_host_baz_url);

  // WebUI URLs should result in SiteURLs with the full scheme and hostname
  // of the WebUI URL.
  EXPECT_EQ(webui_tld_url, webui_tld_site_info.site_url());
  EXPECT_EQ(webui_host_bar_url, webui_host_bar_site_info.site_url());
  EXPECT_EQ(webui_host_baz_url, webui_host_baz_site_info.site_url());

  // WebUI URLs should use their TLD for ProcessLockURLs.
  EXPECT_EQ(webui_tld_url, webui_tld_site_info.process_lock_url());
  EXPECT_EQ(webui_tld_url, webui_host_bar_site_info.process_lock_url());
  EXPECT_EQ(webui_tld_url, webui_host_baz_site_info.process_lock_url());
}

TEST_F(SiteInstanceTest, ErrorPage) {
  const GURL non_error_page_url("http://foo.com");
  const GURL error_page_url(kUnreachableWebDataURL);

  // Verify that error SiteInfos are marked by is_error_page() set to true and
  // are not cross origin isolated.
  const auto error_site_info =
      SiteInfo::CreateForErrorPage(CreateStoragePartitionConfigForTesting(),
                                   /*is_guest=*/false, /*is_fenced=*/false,
                                   WebExposedIsolationInfo::CreateNonIsolated(),
                                   WebExposedIsolationLevel::kNotIsolated);
  EXPECT_TRUE(error_site_info.is_error_page());
  EXPECT_FALSE(error_site_info.web_exposed_isolation_info().is_isolated());
  EXPECT_FALSE(error_site_info.is_guest());

  // Verify that non-error URLs don't generate error page SiteInfos.
  const auto instance =
      SiteInstanceImpl::CreateForTesting(context(), non_error_page_url);
  EXPECT_NE(instance->GetSiteInfo(), error_site_info);

  // Verify that an error page URL results in error page SiteInfos.
  const auto error_instance =
      SiteInstanceImpl::CreateForTesting(context(), error_page_url);
  EXPECT_EQ(error_instance->GetSiteInfo(), error_site_info);
  EXPECT_FALSE(error_instance->IsCrossOriginIsolated());

  // Verify that deriving a SiteInfo for an error page URL always returns
  // an error page SiteInfo.
  EXPECT_EQ(error_site_info, instance->DeriveSiteInfo(
                                 UrlInfo::CreateForTesting(error_page_url)));

  // Verify GetRelatedSiteInstance() called with an error page URL always
  // returns an error page SiteInfo.
  const auto related_instance =
      instance->GetRelatedSiteInstance(error_page_url);
  EXPECT_EQ(
      error_site_info,
      static_cast<SiteInstanceImpl*>(related_instance.get())->GetSiteInfo());
}

TEST_F(SiteInstanceTest, RelatedSitesInheritStoragePartitionConfig) {
  const GURL test_url("https://example.com");

  // Create a UrlInfo for test_url loaded in a special StoragePartition.
  const auto non_default_partition_config =
      CreateStoragePartitionConfigForTesting(
          /*in_memory=*/false, /*partition_domain=*/"test_partition");
  const UrlInfo partitioned_url_info(
      UrlInfoInit(test_url).WithStoragePartitionConfig(
          non_default_partition_config));

  // Create a SiteInstance for test_url in the special StoragePartition, and
  // verify that the StoragePartition is correct.
  const auto partitioned_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), partitioned_url_info,
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);
  EXPECT_EQ(non_default_partition_config,
            static_cast<SiteInstanceImpl*>(partitioned_instance.get())
                ->GetSiteInfo()
                .storage_partition_config());

  // Create a related SiteInstance that doesn't specify a
  // StoragePartitionConfig and make sure the StoragePartition gets propagated.
  const auto related_instance =
      partitioned_instance->GetRelatedSiteInstance(test_url);
  EXPECT_EQ(non_default_partition_config,
            static_cast<SiteInstanceImpl*>(related_instance.get())
                ->GetSiteInfo()
                .storage_partition_config());
}

TEST_F(SiteInstanceTest, GetNonOriginKeyedEquivalentPreservesIsPdf) {
  auto origin_isolation_request = static_cast<UrlInfo::OriginIsolationRequest>(
      UrlInfo::OriginIsolationRequest::kOriginAgentClusterByHeader |
      UrlInfo::OriginIsolationRequest::kRequiresOriginKeyedProcessByHeader);
  UrlInfo url_info_pdf_with_oac(
      UrlInfoInit(GURL("https://foo.com/test.pdf"))
          .WithOriginIsolationRequest(origin_isolation_request)
          .WithIsPdf(true));
  SiteInfo site_info_pdf_with_origin_key =
      SiteInfo::Create(IsolationContext(context()), url_info_pdf_with_oac);
  SiteInfo site_info_pdf_no_origin_key =
      site_info_pdf_with_origin_key.GetNonOriginKeyedEquivalentForMetrics(
          IsolationContext(context()));

  // Verify that the non-origin-keyed equivalent still has the is_pdf flag set
  // but has the is_origin_keyed flag cleared.
  EXPECT_TRUE(site_info_pdf_with_origin_key.is_pdf());
  EXPECT_TRUE(site_info_pdf_no_origin_key.is_pdf());
  EXPECT_TRUE(site_info_pdf_with_origin_key.requires_origin_keyed_process());
  EXPECT_FALSE(site_info_pdf_no_origin_key.requires_origin_keyed_process());
}

// This test makes sure that if we create a SiteInfo with a UrlInfo where
// kOriginAgentClusterByHeader is set but kRequiresOriginKeyedProcessByHeader is
// not, that the resulting SiteInfo does not have
// `requires_origin_keyed_process_` true.
TEST_F(SiteInstanceTest, SiteInfoDetermineProcessLock_OriginAgentCluster) {
  GURL a_foo_url("https://a.foo.com/");
  GURL foo_url("https://foo.com");

  // In the test below, it's important for the IsolationContext to have a
  // non-null BrowsingInstanceId, otherwise the call to
  // ChildProcessSecurityPolicyImpl::GetMatchingProcessIsolatedOrigin() will
  // skip over the check for OAC process isolated origins, which is required for
  // this test to operate.
  SiteInfo site_info_for_a_foo = SiteInfo::Create(
      IsolationContext(
          BrowsingInstanceId::FromUnsafeValue(42), context(),
          /*is_guest=*/false, /*is_fenced=*/false,
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              context())),
      UrlInfo(UrlInfoInit(a_foo_url).WithOriginIsolationRequest(
          UrlInfo::OriginIsolationRequest::kOriginAgentClusterByHeader)));
  EXPECT_TRUE(
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_EQ(foo_url, site_info_for_a_foo.process_lock_url());
  EXPECT_FALSE(site_info_for_a_foo.requires_origin_keyed_process());
}

TEST_F(SiteInstanceTest, ShouldAssignSiteForAboutBlank) {
  const GURL about_blank(url::kAboutBlankURL);
  url::Origin example_origin =
      url::Origin::Create(GURL("https://www.example.com"));
  url::Origin opaque_with_precursor_origin =
      example_origin.DeriveNewOpaqueOrigin();
  url::Origin opaque_unique_origin;

  UrlInfo blank_no_origin = UrlInfo(UrlInfoInit(about_blank));
  UrlInfo blank_with_normal_origin(
      UrlInfoInit(about_blank).WithOrigin(example_origin));
  UrlInfo blank_with_opaque_origin_and_precursor(
      UrlInfoInit(about_blank).WithOrigin(opaque_with_precursor_origin));
  UrlInfo blank_with_opaque_unique_origin(
      UrlInfo(UrlInfoInit(about_blank).WithOrigin(opaque_unique_origin)));

  // about:blank with no associated origin should not assign a site.
  EXPECT_FALSE(SiteInstanceImpl::ShouldAssignSiteForUrlInfo(blank_no_origin));

  // about:blank with an origin *should* assign a site.
  EXPECT_TRUE(
      SiteInstanceImpl::ShouldAssignSiteForUrlInfo(blank_with_normal_origin));

  // Similarly, about:blank with an opaque origin that has a valid precursor
  // origin also needs to assign a site.
  EXPECT_TRUE(SiteInstanceImpl::ShouldAssignSiteForUrlInfo(
      blank_with_opaque_origin_and_precursor));

  // about:blank with an opaque unique origin does not need to assign a site.
  EXPECT_FALSE(SiteInstanceImpl::ShouldAssignSiteForUrlInfo(
      blank_with_opaque_unique_origin));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceIdentity) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url)));

  EXPECT_EQ(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceCrossSite) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(GURL("https://other-example.com"))));

  // Without full Site Isolation, we'll group different sites in the default
  // SiteInstance.
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(derived_instance.get(), base_instance.get());
    return;
  }

  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceIdenticalCoopOriginSameSite) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(),
      UrlInfo(UrlInfoInit(test_url).WithCommonCoopOrigin(
          url::Origin::Create(test_url))),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url).WithCommonCoopOrigin(
          url::Origin::Create(test_url))));
  EXPECT_EQ(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceIdenticalCoopOriginCrossSite) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(),
      UrlInfo(UrlInfoInit(test_url).WithCommonCoopOrigin(
          url::Origin::Create(test_url))),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);

  // COOP common origin might differ from the frame's actual origin (for
  // example for cross-origin subframes), so we verify that this case is handled
  // properly.
  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(GURL("https://other-example.com"))
                  .WithCommonCoopOrigin(url::Origin::Create(test_url))));

  // Without full Site Isolation, we'll group different sites in the default
  // SiteInstance.
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(derived_instance.get(), base_instance.get());
    return;
  }

  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceDifferentCoopOrigin) {
  const GURL test_url("https://example.com");

  // Start without a COOP origin.
  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url).WithCommonCoopOrigin(
          url::Origin::Create(test_url))));
  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_FALSE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceIdenticalCrossOriginIsolation) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(),
      UrlInfo(UrlInfoInit(test_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolated(
              url::Origin::Create(test_url)))),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolated(
              url::Origin::Create(test_url)))));
  EXPECT_EQ(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, CoopRelatedSiteInstanceDifferentCrossOriginIsolation) {
  const GURL test_url("https://example.com");

  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolated(
              url::Origin::Create(test_url)))));
  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_FALSE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
}

TEST_F(SiteInstanceTest, GroupTokensBuilding) {
  const GURL test_url("https://example.com");
  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  base::UnguessableToken browsing_instance_token =
      base_instance->browsing_instance_token();
  base::UnguessableToken coop_related_group_token =
      base_instance->coop_related_group_token();
  EXPECT_NE(browsing_instance_token, coop_related_group_token);
}

TEST_F(SiteInstanceTest, GroupTokensRelatedSiteInstances) {
  const GURL test_url("https://example.com");
  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto derived_instance = base_instance->GetRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(GURL("https://other-example.com"))));

  // Without full Site Isolation, we'll group different sites in the default
  // SiteInstance.
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_EQ(derived_instance.get(), base_instance.get());
    return;
  }

  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_TRUE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_EQ(derived_instance->browsing_instance_token(),
            base_instance->browsing_instance_token());
  EXPECT_EQ(derived_instance->coop_related_group_token(),
            base_instance->coop_related_group_token());
}

TEST_F(SiteInstanceTest, GroupTokensCoopRelatedSiteInstances) {
  const GURL test_url("https://example.com");
  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  // Derive a SiteInstance that lives in the same CoopRelatedGroup but a
  // different BrowsingInstance. Provide a different WebExposedIsolationInfo to
  // make sure we do not reuse the BrowsingInstance.
  const auto derived_instance = base_instance->GetCoopRelatedSiteInstanceImpl(
      UrlInfo(UrlInfoInit(test_url).WithWebExposedIsolationInfo(
          WebExposedIsolationInfo::CreateIsolated(
              url::Origin::Create(test_url)))));
  EXPECT_NE(derived_instance.get(), base_instance.get());
  EXPECT_FALSE(derived_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_TRUE(derived_instance->IsCoopRelatedSiteInstance(base_instance.get()));
  EXPECT_NE(derived_instance->browsing_instance_token(),
            base_instance->browsing_instance_token());
  EXPECT_EQ(derived_instance->coop_related_group_token(),
            base_instance->coop_related_group_token());
}

TEST_F(SiteInstanceTest, GroupTokensUnrelatedSiteInstances) {
  const GURL test_url("https://example.com");
  const auto base_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  const auto other_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo(UrlInfoInit(test_url)), /*is_guest=*/false,
      /*is_fenced=*/false, /*is_fixed_storage_partition=*/false);

  EXPECT_NE(other_instance.get(), base_instance.get());
  EXPECT_FALSE(other_instance->IsRelatedSiteInstance(base_instance.get()));
  EXPECT_FALSE(other_instance->IsCoopRelatedSiteInstance(base_instance.get()));
  EXPECT_NE(other_instance->browsing_instance_token(),
            base_instance->browsing_instance_token());
  EXPECT_NE(other_instance->coop_related_group_token(),
            base_instance->coop_related_group_token());
}

namespace {

class SiteInstanceGotProcessAndSiteBrowserClient
    : public TestContentBrowserClient {
 public:
  SiteInstanceGotProcessAndSiteBrowserClient() {}

  void SiteInstanceGotProcessAndSite(SiteInstance* site_instance) override {
    call_count_++;
  }

  int call_count() { return call_count_; }

 private:
  int call_count_ = 0;
};

}  // namespace

// Check that there's one call to SiteInstanceGotProcessAndSite() when a
// SiteInstance gets a process first and a site second.
TEST_F(SiteInstanceTest, SiteInstanceGotProcessAndSite_ProcessThenSite) {
  SiteInstanceGotProcessAndSiteBrowserClient custom_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&custom_client);

  const auto site_instance = SiteInstanceImpl::Create(context());
  EXPECT_FALSE(site_instance->HasSite());
  EXPECT_EQ(0, custom_client.call_count());

  // Assigning a process shouldn't call SiteInstanceGotProcessAndSite(), since
  // there's no site yet.
  EXPECT_FALSE(site_instance->HasProcess());
  site_instance->GetProcess();
  EXPECT_TRUE(site_instance->HasProcess());
  EXPECT_EQ(0, custom_client.call_count());

  // Now, assign a site and expect a call to SiteInstanceGotProcessAndSite().
  site_instance->SetSite(UrlInfo::CreateForTesting(GURL("https://foo.com")));
  EXPECT_EQ(1, custom_client.call_count());

  // Repeated calls to get a process shouldn't produce new calls.
  site_instance->GetProcess();
  EXPECT_EQ(1, custom_client.call_count());

  SetBrowserClientForTesting(regular_client);
}

// Same as above, but now SiteInstance gets a site first and a process second.
TEST_F(SiteInstanceTest, SiteInstanceGotProcessAndSite_SiteThenProcess) {
  SiteInstanceGotProcessAndSiteBrowserClient custom_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&custom_client);

  const auto site_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo::CreateForTesting(GURL("https://foo.com")),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);
  EXPECT_TRUE(site_instance->HasSite());
  EXPECT_FALSE(site_instance->HasProcess());
  EXPECT_EQ(0, custom_client.call_count());

  site_instance->GetProcess();
  EXPECT_EQ(1, custom_client.call_count());

  // Repeated calls to get a process shouldn't produce new calls.
  site_instance->GetProcess();
  EXPECT_EQ(1, custom_client.call_count());

  // Expect a new call if a SiteInstance's RenderProcessHost gets destroyed
  // and replaced with a new one.
  EXPECT_TRUE(site_instance->HasProcess());
  site_instance->GetProcess()->Cleanup();
  EXPECT_FALSE(site_instance->HasProcess());
  site_instance->GetProcess();
  EXPECT_TRUE(site_instance->HasProcess());
  EXPECT_EQ(2, custom_client.call_count());

  SetBrowserClientForTesting(regular_client);
}

// Check that SiteInstanceGotProcessAndSite() works properly in
// process-per-site mode.
TEST_F(SiteInstanceTest, SiteInstanceGotProcessAndSite_ProcessPerSite) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  SiteInstanceGotProcessAndSiteBrowserClient custom_client;
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&custom_client);

  const auto site_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo::CreateForTesting(GURL("https://foo.com")),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);
  EXPECT_TRUE(site_instance->HasSite());
  EXPECT_FALSE(site_instance->HasProcess());
  EXPECT_EQ(0, custom_client.call_count());

  site_instance->GetProcess();
  EXPECT_EQ(1, custom_client.call_count());

  // Create another SiteInstance for the same site, which should reuse the
  // process from the first SiteInstance, since we're in process-per-site mode.
  const auto second_instance = SiteInstanceImpl::CreateForUrlInfo(
      context(), UrlInfo::CreateForTesting(GURL("https://foo.com")),
      /*is_guest=*/false, /*is_fenced=*/false,
      /*is_fixed_storage_partition=*/false);

  // In process-per-site mode, HasProcess() returns true even if the
  // SiteInstance hasn't gone through SetProcessInternal(). However,
  // SiteInstanceGotProcess() shouldn't have been called on it yet.
  EXPECT_TRUE(second_instance->HasProcess());
  EXPECT_EQ(1, custom_client.call_count());

  // Assigning a process for the second SiteInstance should trigger a call to
  // SiteInstanceGotProcess(), even though the process is reused.
  second_instance->GetProcess();
  EXPECT_EQ(second_instance->GetProcess(), site_instance->GetProcess());
  EXPECT_EQ(2, custom_client.call_count());

  SetBrowserClientForTesting(regular_client);
}

}  // namespace content
