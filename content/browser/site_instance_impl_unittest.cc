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
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
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
  return SiteInfo(
      GURL("https://www.foo.com"), process_lock_url,
      requires_origin_keyed_process, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */);
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

  void SiteInstanceDeleting(content::SiteInstance* site_instance) override {
    site_instance_delete_count_++;
    // Infer deletion of the browsing instance.
    if (static_cast<SiteInstanceImpl*>(site_instance)
            ->browsing_instance_->HasOneRef()) {
      browsing_instance_delete_count_++;
    }
  }

  int GetAndClearSiteInstanceDeleteCount() {
    int result = site_instance_delete_count_;
    site_instance_delete_count_ = 0;
    return result;
  }

  int GetAndClearBrowsingInstanceDeleteCount() {
    int result = browsing_instance_delete_count_;
    browsing_instance_delete_count_ = 0;
    return result;
  }

 private:
  int privileged_process_id_ = -1;

  int site_instance_delete_count_ = 0;
  int browsing_instance_delete_count_ = 0;
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
  }

  void TearDown() override {
    // Ensure that no RenderProcessHosts are left over after the tests.
    EXPECT_TRUE(RenderProcessHost::AllHostsIterator().IsAtEnd());

    SetBrowserClientForTesting(old_browser_client_);
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
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
  auto site_info_1_with_isolation_request = SiteInfo(
      GURL("https://www.foo.com") /* site_url */,
      GURL("https://foo.com") /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      true /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */);
  EXPECT_TRUE(
      site_info_1.IsSamePrincipalWith(site_info_1_with_isolation_request));
  EXPECT_EQ(site_info_1, site_info_1_with_isolation_request);

  // Check that SiteInfos with differing values of `is_jit_disabled` are not
  // considered same-principal.
  auto site_info_1_with_jit_disabled = SiteInfo(
      GURL("https://www.foo.com") /* site_url */,
      GURL("https://foo.com") /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      true /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */);
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1_with_jit_disabled));

  // Check that SiteInfos with differing values of `is_pdf` are not considered
  // same-principal.
  auto site_info_1_with_pdf = SiteInfo(
      GURL("https://www.foo.com") /* site_url */,
      GURL("https://foo.com") /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, true /* is_pdf */, false /* is_fenced */);
  EXPECT_FALSE(site_info_1.IsSamePrincipalWith(site_info_1_with_pdf));

  auto site_info_1_with_is_fenced = SiteInfo(
      GURL("https://www.foo.com") /* site_url */,
      GURL("https://foo.com") /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, true /* is_fenced */);
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
  EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());

  NavigationEntryImpl* e1 = new NavigationEntryImpl(
      instance, url, Referrer(), /* initiator_origin= */ absl::nullopt,
      /* initiator_base_url= */ absl::nullopt, std::u16string(),
      ui::PAGE_TRANSITION_LINK, false, nullptr /* blob_url_loader_factory */,
      false /* is_initial_entry */);

  // Redundantly setting e1's SiteInstance shouldn't affect the ref count.
  e1->set_site_instance(instance);
  EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(0, browser_client()->GetAndClearBrowsingInstanceDeleteCount());

  // Add a second reference
  NavigationEntryImpl* e2 = new NavigationEntryImpl(
      instance, url, Referrer(), /* initiator_origin= */ absl::nullopt,
      /* initiator_base_url= */ absl::nullopt, std::u16string(),
      ui::PAGE_TRANSITION_LINK, false, nullptr /* blob_url_loader_factory */,
      false /* is_initial_entry */);

  instance = nullptr;
  EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(0, browser_client()->GetAndClearBrowsingInstanceDeleteCount());

  // Now delete both entries and be sure the SiteInstance goes away.
  delete e1;
  EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(0, browser_client()->GetAndClearBrowsingInstanceDeleteCount());
  delete e2;
  // instance is now deleted
  EXPECT_EQ(1, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(1, browser_client()->GetAndClearBrowsingInstanceDeleteCount());
  // browsing_instance is now deleted

  // Ensure that instances are deleted when their RenderViewHosts are gone.
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  {
    std::unique_ptr<WebContents> web_contents(
        WebContents::Create(WebContents::CreateParams(
            browser_context.get(),
            SiteInstance::Create(browser_context.get()))));
    EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());
    EXPECT_EQ(0, browser_client()->GetAndClearBrowsingInstanceDeleteCount());
  }

  // Make sure that we flush any messages related to the above WebContentsImpl
  // destruction.
  DrainMessageLoop();

  EXPECT_EQ(1, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(1, browser_client()->GetAndClearBrowsingInstanceDeleteCount());
  // contents is now deleted, along with instance and browsing_instance
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

  EXPECT_EQ(AreDefaultSiteInstancesEnabled(),
            site_instance->IsDefaultSiteInstance());

  site_instance.reset();

  EXPECT_EQ(1, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(1, browser_client()->GetAndClearBrowsingInstanceDeleteCount());
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

  // Pages are irrelevant.
  GURL test_url = GURL("http://www.google.com/index.html");
  GURL site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("http://google.com"), site_url);
  EXPECT_EQ("http", site_url.scheme());
  EXPECT_EQ("google.com", site_url.host());

  // Ports are irrelevant.
  test_url = GURL("https://www.google.com:8080");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(GURL("https://google.com"), site_url);

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
  EXPECT_EQ(GURL("http://[::1]"), site_url);
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

  // Data URLs should include the whole URL, except for the hash.
  test_url = GURL("data:text/html,foo");
  site_url = GetSiteForURL(test_url);
  EXPECT_EQ(test_url, site_url);
  EXPECT_EQ("data", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());
  test_url = GURL("data:text/html,foo#bar");
  site_url = GetSiteForURL(test_url);
  EXPECT_FALSE(site_url.has_ref());
  EXPECT_NE(test_url, site_url);
  EXPECT_TRUE(site_url.EqualsIgnoringRef(test_url));

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
  EXPECT_EQ(GURL("https://chromium.org"), site_url);

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
                                   /*is_guest=*/false, /*is_fenced=*/false);
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
    auto site_info = SiteInfo::CreateForTesting(isolation_context, test_url);
    EXPECT_EQ(nonapp_site_url, site_info.process_lock_url());
    EXPECT_EQ(app_url, site_info.site_url());
  }

  SiteInfo expected_site_info(
      app_url /* site_url */, nonapp_site_url /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */);

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
      /*is_guest=*/false, /*is_fenced=*/false);

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
      /*is_guest=*/false, /*is_fenced=*/false);
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
      /*is_guest=*/false, /*is_fenced=*/false);

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
      /*is_guest=*/false, /*is_fenced=*/false);
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
      /*is_guest=*/false, /*is_fenced=*/false);
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
      webui_host->GetID(), BINDINGS_POLICY_WEB_UI);

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

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_foo_url));
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_bar_url));
}

TEST_F(SiteInstanceTest, IsolatedOriginsWithPort) {
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL isolated_foo_with_port("http://isolated.foo.com:12345");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  {
    base::test::MockLog mock_log;
    EXPECT_CALL(
        mock_log,
        Log(::logging::LOG_ERROR, testing::_, testing::_, testing::_,
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

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_foo_url));
  policy->RemoveIsolatedOriginForTesting(
      url::Origin::Create(isolated_foo_with_port));
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

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_url));
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_ip));
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

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_foo_url));
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

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(foo_url));
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(baz_bar_foo_url));
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

  SiteInfo expected_site_info(
      app_url /* site_url */, original_url /* process_lock_url */,
      false /* requires_origin_keyed_process */, false /* is_sandboxed */,
      UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */);

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

namespace {

ProcessLock ProcessLockFromString(const std::string& url) {
  return ProcessLock::FromSiteInfo(SiteInfo(
      GURL(url), GURL(url), false /* requires_origin_keyed_process */,
      false /* is_sandboxed */, UrlInfo::kInvalidUniqueSandboxId,
      CreateStoragePartitionConfigForTesting(),
      WebExposedIsolationInfo::CreateNonIsolated(), false /* is_guest */,
      false /* does_site_request_dedicated_process_for_coop */,
      false /* is_jit_disabled */, false /* is_pdf */, false /* is_fenced */));
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
    CustomBrowserClient(const GURL& url_to_modify, const GURL& url_to_return)
        : EffectiveURLContentBrowserClient(url_to_modify,
                                           url_to_return,
                                           false) {}

    void set_should_not_assign_url(const GURL& url) {
      should_not_assign_url_ = url;
    }

    bool ShouldAssignSiteForURL(const GURL& url) override {
      return url != should_not_assign_url_;
    }

   private:
    GURL should_not_assign_url_;
  };

  const GURL kNonIsolatedUrl("https://bar.com/");
  const GURL kIsolatedUrl("https://isolated.com/");
  const GURL kFileUrl("file:///C:/Downloads/");
  const GURL kCustomUrl("http://custom.foo.com");
  const GURL kCustomAppUrl(std::string(kCustomStandardScheme) + "://custom");
  CustomBrowserClient modified_client(kCustomUrl, kCustomAppUrl);
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

  // Test the "do not assign site" case with an effective URL.
  modified_client.set_should_not_assign_url(kCustomUrl);

  if (instance5->IsDefaultSiteInstance()) {
    // Verify that the default SiteInstance is no longer a site match
    // with |kCustomUrl| because this URL now requires a SiteInstance that
    // does not have its site set.
    EXPECT_FALSE(instance5->DoesSiteInfoForURLMatch(
        UrlInfo::CreateForTesting(kCustomUrl)));
    EXPECT_FALSE(instance5->IsSameSiteWithURL(kCustomUrl));
  }

  // Verify that |kCustomUrl| will always construct a SiteInstance without
  // a site set now.
  auto instance6 = SiteInstanceImpl::CreateForTesting(context(), kCustomUrl);
  EXPECT_FALSE(instance6->IsDefaultSiteInstance());
  EXPECT_FALSE(instance6->HasSite());
  EXPECT_FALSE(instance6->DoesSiteInfoForURLMatch(
      UrlInfo::CreateForTesting(kCustomUrl)));
  EXPECT_FALSE(instance6->IsSameSiteWithURL(kCustomUrl));

  SetBrowserClientForTesting(regular_client);
}

TEST_F(SiteInstanceTest, CreateForGuest) {
  const GURL kGuestUrl(std::string(kGuestScheme) + "://abc123/path");

  // Verify that a SiteInstance created with CreateForUrlInfo() is not
  // considered a <webview> guest and has the path removed for the site URL like
  // any other standard URL.
  auto instance1 = SiteInstanceImpl::CreateForTesting(context(), kGuestUrl);
  EXPECT_FALSE(instance1->IsGuest());
  if (AreDefaultSiteInstancesEnabled()) {
    EXPECT_TRUE(instance1->IsDefaultSiteInstance());
  } else {
    EXPECT_NE(kGuestUrl, instance1->GetSiteURL());
    EXPECT_EQ(GURL(std::string(kGuestScheme) + "://abc123/"),
              instance1->GetSiteURL());
  }

  // Verify that a SiteInstance created with CreateForGuest() is considered
  // a <webview> guest.  Without site isolation for guests, its site URL
  // should reflect the guest's StoragePartition configuration.
  const StoragePartitionConfig kGuestConfig = StoragePartitionConfig::Create(
      context(), "appid", "partition_name", /*in_memory=*/false);
  const GURL kGuestSiteUrl(std::string(kGuestScheme) +
                           "://appid/persist?partition_name#nofallback");
  auto instance2 = SiteInstanceImpl::CreateForGuest(context(), kGuestConfig);
  EXPECT_TRUE(instance2->IsGuest());
  EXPECT_EQ(instance2->GetStoragePartitionConfig(), kGuestConfig);
  if (!SiteIsolationPolicy::IsSiteIsolationForGuestsEnabled())
    EXPECT_EQ(kGuestSiteUrl, instance2->GetSiteURL());
}

// TODO(https://crbug.com/1377466): Test is flaky for android builders.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DoesSiteRequireDedicatedProcess \
  DISABLED_DoesSiteRequireDedicatedProcess
#else
#define MAYBE_DoesSiteRequireDedicatedProcess DoesSiteRequireDedicatedProcess
#endif
TEST_F(SiteInstanceTest, MAYBE_DoesSiteRequireDedicatedProcess) {
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
    EXPECT_TRUE(DoesURLRequireDedicatedProcess(isolation_context, GURL(url)));
  }

  for (const auto& url : kUrlsThatDoNotRequireADedicatedProcess) {
    EXPECT_EQ(AreAllSitesIsolatedForTesting(),
              DoesURLRequireDedicatedProcess(isolation_context, GURL(url)));
  }
  SetBrowserClientForTesting(regular_client);
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
                                   /*is_guest=*/false, /*is_fenced=*/false);
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
      /*is_guest=*/false, /*is_fenced=*/false);
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
      UrlInfo::OriginIsolationRequest::kOriginAgentCluster |
      UrlInfo::OriginIsolationRequest::kRequiresOriginKeyedProcess);
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
// kOriginAgentCluster is set but kRequiresOriginKeyedProcess is not, that the
// resulting SiteInfo does not have `requires_origin_keyed_process_` true.
TEST_F(SiteInstanceTest, SiteInfoDetermineProcessLock_OriginAgentCluster) {
  GURL a_foo_url("https://a.foo.com/");
  GURL foo_url("https://foo.com");

  // In the test below, it's important for the IsolationContext to have a
  // non-null BrowsingInstanceId, otherwise the call to
  // ChildProcessSecurityPolicyImpl::GetMatchingProcessIsolatedOrigin() will
  // skip over the check for OAC process isolated origins, which is required for
  // this test to operate.
  SiteInfo site_info_for_a_foo = SiteInfo::Create(
      IsolationContext(BrowsingInstanceId::FromUnsafeValue(42), context(),
                       /*is_guest=*/false, /*is_fenced=*/false),
      UrlInfo(UrlInfoInit(a_foo_url).WithOriginIsolationRequest(
          UrlInfo::OriginIsolationRequest::kOriginAgentCluster)));
  EXPECT_TRUE(
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_EQ(foo_url, site_info_for_a_foo.process_lock_url());
  EXPECT_FALSE(site_info_for_a_foo.requires_origin_keyed_process());
}

}  // namespace content
