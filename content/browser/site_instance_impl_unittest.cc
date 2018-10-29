// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace content {
namespace {

GURL GetWebUIURL(std::string host) {
  return GURL(std::string(kChromeUIScheme) + "://" + host);
}

}  // namespace

const char kPrivilegedScheme[] = "privileged";

class SiteInstanceTestBrowserClient : public TestContentBrowserClient {
 public:
  SiteInstanceTestBrowserClient()
      : privileged_process_id_(-1),
        site_instance_delete_count_(0),
        browsing_instance_delete_count_(0) {
    WebUIControllerFactory::RegisterFactory(
        ContentWebUIControllerFactory::GetInstance());
  }

  ~SiteInstanceTestBrowserClient() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(
        ContentWebUIControllerFactory::GetInstance());
  }

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
  int privileged_process_id_;

  int site_instance_delete_count_;
  int browsing_instance_delete_count_;
};

class SiteInstanceTest : public testing::Test {
 public:
  SiteInstanceTest() : old_browser_client_(nullptr) {}

  void SetUp() override {
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    url::AddStandardScheme(kPrivilegedScheme, url::SCHEME_WITH_HOST);
    url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITH_HOST);

    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &rph_factory_);
  }

  void TearDown() override {
    // Ensure that no RenderProcessHosts are left over after the tests.
    EXPECT_TRUE(RenderProcessHost::AllHostsIterator().IsAtEnd());

    SetBrowserClientForTesting(old_browser_client_);
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);

    // http://crbug.com/143565 found SiteInstanceTest leaking an
    // AppCacheDatabase. This happens because some part of the test indirectly
    // calls StoragePartitionImplMap::PostCreateInitialization(), which posts
    // a task to the IO thread to create the AppCacheDatabase. Since the
    // message loop is not running, the AppCacheDatabase ends up getting
    // created when DrainMessageLoop() gets called at the end of a test case.
    // Immediately after, the test case ends and the AppCacheDatabase gets
    // scheduled for deletion. Here, call DrainMessageLoop() again so the
    // AppCacheDatabase actually gets deleted.
    DrainMessageLoop();

    ResetSchemesAndOriginsWhitelist();
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

 private:
  TestBrowserThreadBundle test_browser_thread_bundle_;

  SiteInstanceTestBrowserClient browser_client_;
  ContentBrowserClient* old_browser_client_;
  MockRenderProcessHostFactory rph_factory_;
};

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
      instance, url, Referrer(), base::string16(), ui::PAGE_TRANSITION_LINK,
      false, nullptr /* blob_url_loader_factory */);

  // Redundantly setting e1's SiteInstance shouldn't affect the ref count.
  e1->set_site_instance(instance);
  EXPECT_EQ(0, browser_client()->GetAndClearSiteInstanceDeleteCount());
  EXPECT_EQ(0, browser_client()->GetAndClearBrowsingInstanceDeleteCount());

  // Add a second reference
  NavigationEntryImpl* e2 = new NavigationEntryImpl(
      instance, url, Referrer(), base::string16(), ui::PAGE_TRANSITION_LINK,
      false, nullptr /* blob_url_loader_factory */);

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

// Test to ensure GetProcess returns and creates processes correctly.
TEST_F(SiteInstanceTest, GetProcess) {
  // Ensure that GetProcess returns a process.
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  std::unique_ptr<RenderProcessHost> host1;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));
  host1.reset(instance->GetProcess());
  EXPECT_TRUE(host1.get() != nullptr);

  // Ensure that GetProcess creates a new process.
  scoped_refptr<SiteInstanceImpl> instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  std::unique_ptr<RenderProcessHost> host2(instance2->GetProcess());
  EXPECT_TRUE(host2.get() != nullptr);
  EXPECT_NE(host1.get(), host2.get());

  DrainMessageLoop();
}

// Test to ensure SetSite and site() work properly.
TEST_F(SiteInstanceTest, SetSite) {
  TestBrowserContext context;

  scoped_refptr<SiteInstanceImpl> instance(SiteInstanceImpl::Create(&context));
  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  instance->SetSite(GURL("http://www.google.com/index.html"));
  EXPECT_EQ(GURL("http://google.com"), instance->GetSiteURL());

  EXPECT_TRUE(instance->HasSite());

  DrainMessageLoop();
}

// Test to ensure GetSiteForURL properly returns sites for URLs.
TEST_F(SiteInstanceTest, GetSiteForURL) {
  TestBrowserContext context;

  // Pages are irrelevant.
  GURL test_url = GURL("http://www.google.com/index.html");
  GURL site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://google.com"), site_url);
  EXPECT_EQ("http", site_url.scheme());
  EXPECT_EQ("google.com", site_url.host());

  // Ports are irrelevant.
  test_url = GURL("https://www.google.com:8080");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("https://google.com"), site_url);

  // Punycode is canonicalized.
  test_url = GURL("http://☃snowperson☃.net:333/");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://xn--snowperson-di0gka.net"), site_url);

  // Username and password are stripped out.
  test_url = GURL("ftp://username:password@ftp.chromium.org/files/README");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("ftp://chromium.org"), site_url);

  // Literal IP addresses of any flavor are okay.
  test_url = GURL("http://127.0.0.1/a.html");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://127.0.0.1"), site_url);
  EXPECT_EQ("127.0.0.1", site_url.host());

  test_url = GURL("http://2130706433/a.html");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://127.0.0.1"), site_url);
  EXPECT_EQ("127.0.0.1", site_url.host());

  test_url = GURL("http://[::1]:2/page.html");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://[::1]"), site_url);
  EXPECT_EQ("[::1]", site_url.host());

  // Hostnames without TLDs are okay.
  test_url = GURL("http://foo/a.html");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://foo"), site_url);
  EXPECT_EQ("foo", site_url.host());

  // File URLs should include the scheme.
  test_url = GURL("file:///C:/Downloads/");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Some file URLs have hosts in the path.  For consistency with Blink (which
  // maps *all* file://... URLs into "file://" origin) such file URLs still need
  // to map into "file:" site URL.  See also https://crbug.com/776160.
  test_url = GURL("file://server/path");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Data URLs should include the whole URL, except for the hash, when Site
  // Isolation is enabled.  Otherwise they just include the scheme.
  test_url = GURL("data:text/html,foo");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  if (AreAllSitesIsolatedForTesting())
    EXPECT_EQ(test_url, site_url);
  else
    EXPECT_EQ(GURL("data:"), site_url);
  EXPECT_EQ("data", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());
  test_url = GURL("data:text/html,foo#bar");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_FALSE(site_url.has_ref());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(test_url, site_url);
    EXPECT_TRUE(site_url.EqualsIgnoringRef(test_url));
  } else {
    EXPECT_EQ(GURL("data:"), site_url);
  }

  // Javascript URLs should include the scheme.
  test_url = GURL("javascript:foo();");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("javascript:"), site_url);
  EXPECT_EQ("javascript", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Blob URLs extract the site from the origin.
  test_url = GURL(
      "blob:gopher://www.ftp.chromium.org/"
      "4d4ff040-6d61-4446-86d3-13ca07ec9ab9");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("gopher://chromium.org"), site_url);

  // Blob URLs with file origin also extract the site from the origin.
  test_url = GURL("blob:file:///1029e5a4-2983-4b90-a585-ed217563acfeb");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("file:"), site_url);
  EXPECT_EQ("file", site_url.scheme());
  EXPECT_FALSE(site_url.has_host());

  // Blob URLs created from a unique origin use the full URL as the site URL
  // when Site Isolation is enabled, except for the hash.  Otherwise they just
  // include the scheme.
  test_url = GURL("blob:null/1029e5a4-2983-4b90-a585-ed217563acfeb");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  if (AreAllSitesIsolatedForTesting())
    EXPECT_EQ(test_url, site_url);
  else
    EXPECT_EQ(GURL("blob:"), site_url);
  test_url = GURL("blob:null/1029e5a4-2983-4b90-a585-ed217563acfeb#foo");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_FALSE(site_url.has_ref());
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(test_url, site_url);
    EXPECT_TRUE(site_url.EqualsIgnoringRef(test_url));
  } else {
    EXPECT_EQ(GURL("blob:"), site_url);
  }

  // Private domains are preserved, appspot being such a site.
  test_url = GURL(
      "blob:http://www.example.appspot.com:44/"
      "4d4ff040-6d61-4446-86d3-13ca07ec9ab9");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://example.appspot.com"), site_url);

  // The site of filesystem URLs is determined by the inner URL.
  test_url = GURL("filesystem:http://www.google.com/foo/bar.html?foo#bar");
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(GURL("http://google.com"), site_url);

  // Guest URLs are special and need to have the path in the site as well,
  // since it affects the StoragePartition configuration.
  std::string guest_url(kGuestScheme);
  guest_url.append("://abc123/path");
  test_url = GURL(guest_url);
  site_url = SiteInstance::GetSiteForURL(&context, test_url);
  EXPECT_EQ(test_url, site_url);

  DrainMessageLoop();
}

// Test that process lock URLs are computed without using effective URLs.
TEST_F(SiteInstanceTest, ProcessLockDoesNotUseEffectiveURL) {
  TestBrowserContext context;
  GURL test_url("https://some.app.foo.com/");
  GURL nonapp_site_url("https://foo.com/");
  GURL app_url("https://app.com/");
  EffectiveURLContentBrowserClient modified_client(test_url, app_url);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());

  // Sanity check that GetSiteForURL's |use_effective_urls| option works
  // properly.  When it's true, the site URL should include both the effective
  // URL's site (app.com) and the original URL's site (foo.com).
  GURL expected_app_site_url(app_url.spec() + "#" + nonapp_site_url.spec());
  {
    GURL site_url = SiteInstanceImpl::GetSiteForURL(
        &context, test_url, false /* use_effective_urls */);
    EXPECT_EQ(nonapp_site_url, site_url);

    site_url = SiteInstanceImpl::GetSiteForURL(&context, test_url,
                                               true /* use_effective_urls */);
    EXPECT_EQ(expected_app_site_url, site_url);
  }

  // New SiteInstance in a new BrowsingInstance with a predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForURL(browser_context.get(), test_url);
    EXPECT_EQ(expected_app_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(nonapp_site_url, site_instance->lock_url());
  }

  // New related SiteInstance from an existing SiteInstance with a
  // predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> bar_site_instance =
        SiteInstanceImpl::CreateForURL(browser_context.get(),
                                       GURL("https://bar.com/"));
    scoped_refptr<SiteInstance> site_instance =
        bar_site_instance->GetRelatedSiteInstance(test_url);
    EXPECT_EQ(expected_app_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(nonapp_site_url,
              static_cast<SiteInstanceImpl*>(site_instance.get())->lock_url());
  }

  // New SiteInstance with a lazily assigned site URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context.get());
    EXPECT_FALSE(site_instance->HasSite());
    site_instance->SetSite(test_url);
    EXPECT_EQ(expected_app_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(nonapp_site_url, site_instance->lock_url());
  }

  SetBrowserClientForTesting(regular_client);
}

// Test of distinguishing URLs from different sites.  Most of this logic is
// tested in RegistryControlledDomainTest.  This test focuses on URLs with
// different schemes or ports.
TEST_F(SiteInstanceTest, IsSameWebSite) {
  TestBrowserContext context;
  GURL url_foo = GURL("http://foo/a.html");
  GURL url_foo2 = GURL("http://foo/b.html");
  GURL url_foo_https = GURL("https://foo/a.html");
  GURL url_foo_port = GURL("http://foo:8080/a.html");
  GURL url_javascript = GURL("javascript:alert(1);");
  GURL url_blank = GURL(url::kAboutBlankURL);

  // Same scheme and port -> same site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_foo, url_foo2));

  // Different scheme -> different site.
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, url_foo, url_foo_https));

  // Different port -> same site.
  // (Changes to document.domain make renderer ignore the port.)
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_foo, url_foo_port));

  // JavaScript links should be considered same site for anything.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_javascript, url_foo));
  EXPECT_TRUE(
      SiteInstance::IsSameWebSite(&context, url_javascript, url_foo_https));
  EXPECT_TRUE(
      SiteInstance::IsSameWebSite(&context, url_javascript, url_foo_port));

  // Navigating to a blank page is considered the same site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_foo, url_blank));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_foo_https, url_blank));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, url_foo_port, url_blank));

  // Navigating from a blank site is not considered to be the same site.
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, url_blank, url_foo));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, url_blank, url_foo_https));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, url_blank, url_foo_port));

  DrainMessageLoop();
}

// Test that two file URLs are considered same-site if they have the same path,
// even if they have different fragments.
TEST_F(SiteInstanceTest, IsSameWebSiteForFileURLs) {
  TestBrowserContext context;

  // Two identical file URLs should be same-site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(
      &context, GURL("file:///foo/bar.html"), GURL("file:///foo/bar.html")));

  // File URLs with the same path but different fragment are considered
  // same-site.
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context,
                                          GURL("file:///foo/bar.html"),
                                          GURL("file:///foo/bar.html#baz")));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context,
                                          GURL("file:///foo/bar.html#baz"),
                                          GURL("file:///foo/bar.html")));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context,
                                          GURL("file:///foo/bar.html#baz"),
                                          GURL("file:///foo/bar.html#qux")));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, GURL("file:///#abc"),
                                          GURL("file:///#def")));

  // Other cases are cross-site.
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, GURL("file:///foo.html"),
                                           GURL("file:///foo/bar.html")));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, GURL("file:///#bar"),
                                           GURL("file:///foo/#bar")));
}

// Test to ensure that there is only one SiteInstance per site in a given
// BrowsingInstance, when process-per-site is not in use.
TEST_F(SiteInstanceTest, OneSiteInstancePerSite) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessPerSite));
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  BrowsingInstance* browsing_instance =
      new BrowsingInstance(browser_context.get());

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(url_a1));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(

      browsing_instance->GetSiteInstanceForURL(url_b1));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same or different
  // browser context) should return a different SiteInstance.
  BrowsingInstance* browsing_instance2 =
      new BrowsingInstance(browser_context.get());
  // Ensure the new SiteInstance is ref counted so that it gets deleted.
  scoped_refptr<SiteInstanceImpl> site_instance_a2_2(
      browsing_instance2->GetSiteInstanceForURL(url_a2));
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_2.get());
  EXPECT_FALSE(
      site_instance_a1->IsRelatedSiteInstance(site_instance_a2_2.get()));

  // The two SiteInstances for http://google.com should not use the same process
  // if process-per-site is not enabled.
  std::unique_ptr<RenderProcessHost> process_a1(site_instance_a1->GetProcess());
  std::unique_ptr<RenderProcessHost> process_a2_2(
      site_instance_a2_2->GetProcess());
  EXPECT_NE(process_a1.get(), process_a2_2.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));

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
  scoped_refptr<BrowsingInstance> browsing_instance =
      new BrowsingInstance(browser_context.get());

  const GURL url_a1("http://www.google.com/1.html");
  scoped_refptr<SiteInstanceImpl> site_instance_a1(
      browsing_instance->GetSiteInstanceForURL(url_a1));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);
  std::unique_ptr<RenderProcessHost> process_a1(site_instance_a1->GetProcess());

  // A separate site should create a separate SiteInstance.
  const GURL url_b1("http://www.yahoo.com/");
  scoped_refptr<SiteInstanceImpl> site_instance_b1(
      browsing_instance->GetSiteInstanceForURL(url_b1));
  EXPECT_NE(site_instance_a1.get(), site_instance_b1.get());
  EXPECT_TRUE(site_instance_a1->IsRelatedSiteInstance(site_instance_b1.get()));

  // Getting the new SiteInstance from the BrowsingInstance and from another
  // SiteInstance in the BrowsingInstance should give the same result.
  EXPECT_EQ(site_instance_b1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_b1));

  // A second visit to the original site should return the same SiteInstance.
  const GURL url_a2("http://www.google.com/2.html");
  EXPECT_EQ(site_instance_a1.get(),
            browsing_instance->GetSiteInstanceForURL(url_a2));
  EXPECT_EQ(site_instance_a1.get(),
            site_instance_a1->GetRelatedSiteInstance(url_a2));

  // A visit to the original site in a new BrowsingInstance (same browser
  // context) should return a different SiteInstance with the same process.
  BrowsingInstance* browsing_instance2 =
      new BrowsingInstance(browser_context.get());
  scoped_refptr<SiteInstanceImpl> site_instance_a1_2(
      browsing_instance2->GetSiteInstanceForURL(url_a1));
  EXPECT_TRUE(site_instance_a1.get() != nullptr);
  EXPECT_NE(site_instance_a1.get(), site_instance_a1_2.get());
  EXPECT_EQ(process_a1.get(), site_instance_a1_2->GetProcess());

  // A visit to the original site in a new BrowsingInstance (different browser
  // context) should return a different SiteInstance with a different process.
  std::unique_ptr<TestBrowserContext> browser_context2(
      new TestBrowserContext());
  BrowsingInstance* browsing_instance3 =
      new BrowsingInstance(browser_context2.get());
  scoped_refptr<SiteInstanceImpl> site_instance_a2_3(
      browsing_instance3->GetSiteInstanceForURL(url_a2));
  EXPECT_TRUE(site_instance_a2_3.get() != nullptr);
  std::unique_ptr<RenderProcessHost> process_a2_3(
      site_instance_a2_3->GetProcess());
  EXPECT_NE(site_instance_a1.get(), site_instance_a2_3.get());
  EXPECT_NE(process_a1.get(), process_a2_3.get());

  // Should be able to see that we do have SiteInstances.
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance2->HasSiteInstance(
      GURL("http://mail.google.com")));  // visited before
  EXPECT_TRUE(browsing_instance->HasSiteInstance(
      GURL("http://mail.yahoo.com")));  // visited before

  // Should be able to see that we don't have SiteInstances.
  EXPECT_FALSE(browsing_instance2->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, same browser context
  EXPECT_FALSE(browsing_instance->HasSiteInstance(
      GURL("https://www.google.com")));  // not visited before
  EXPECT_FALSE(browsing_instance3->HasSiteInstance(
      GURL("http://www.yahoo.com")));  // different BI, different context

  // browsing_instances will be deleted when their SiteInstances are deleted.
  // The processes will be unregistered when the RPH scoped_ptrs go away.

  DrainMessageLoop();
}

// Test to ensure that HasWrongProcessForURL behaves properly for different
// types of URLs.
TEST_F(SiteInstanceTest, HasWrongProcessForURL) {
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  std::unique_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  instance->SetSite(GURL("http://evernote.com/"));
  EXPECT_TRUE(instance->HasSite());

  // Check prior to "assigning" a process to the instance, which is expected
  // to return false due to not being attached to any process yet.
  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://google.com")));

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://evernote.com")));
  EXPECT_FALSE(instance->HasWrongProcessForURL(
      GURL("javascript:alert(document.location.href);")));

  EXPECT_TRUE(instance->HasWrongProcessForURL(GURL("chrome://gpu")));

  // Test that WebUI SiteInstances reject normal web URLs.
  const GURL webui_url("chrome://gpu");
  scoped_refptr<SiteInstanceImpl> webui_instance(
      SiteInstanceImpl::Create(browser_context.get()));
  webui_instance->SetSite(webui_url);
  std::unique_ptr<RenderProcessHost> webui_host(webui_instance->GetProcess());

  // Simulate granting WebUI bindings for the process.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantWebUIBindings(
      webui_host->GetID(), BINDINGS_POLICY_WEB_UI);

  EXPECT_TRUE(webui_instance->HasProcess());
  EXPECT_FALSE(webui_instance->HasWrongProcessForURL(webui_url));
  EXPECT_TRUE(webui_instance->HasWrongProcessForURL(GURL("http://google.com")));
  EXPECT_TRUE(webui_instance->HasWrongProcessForURL(GURL("http://gpu")));

  // WebUI uses process-per-site, so another instance will use the same process
  // even if we haven't called GetProcess yet.  Make sure HasWrongProcessForURL
  // doesn't crash (http://crbug.com/137070).
  scoped_refptr<SiteInstanceImpl> webui_instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  webui_instance2->SetSite(webui_url);
  EXPECT_FALSE(webui_instance2->HasWrongProcessForURL(webui_url));
  EXPECT_TRUE(
      webui_instance2->HasWrongProcessForURL(GURL("http://google.com")));

  DrainMessageLoop();
}

// Test to ensure that HasWrongProcessForURL behaves properly even when
// --site-per-process is used (http://crbug.com/160671).
TEST_F(SiteInstanceTest, HasWrongProcessForURLInSitePerProcess) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  std::unique_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  instance->SetSite(GURL("http://evernote.com/"));
  EXPECT_TRUE(instance->HasSite());

  // Check prior to "assigning" a process to the instance, which is expected
  // to return false due to not being attached to any process yet.
  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://google.com")));

  // The call to GetProcess actually creates a new real process, which works
  // fine, but might be a cause for problems in different contexts.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  EXPECT_FALSE(instance->HasWrongProcessForURL(GURL("http://evernote.com")));
  EXPECT_FALSE(instance->HasWrongProcessForURL(
      GURL("javascript:alert(document.location.href);")));

  EXPECT_TRUE(instance->HasWrongProcessForURL(GURL("chrome://gpu")));

  DrainMessageLoop();
}

// Test that we do not reuse a process in process-per-site mode if it has the
// wrong bindings for its URL.  http://crbug.com/174059.
TEST_F(SiteInstanceTest, ProcessPerSiteWithWrongBindings) {
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  std::unique_ptr<RenderProcessHost> host;
  std::unique_ptr<RenderProcessHost> host2;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  EXPECT_FALSE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());

  // Simulate navigating to a WebUI URL in a process that does not have WebUI
  // bindings.  This already requires bypassing security checks.
  const GURL webui_url("chrome://gpu");
  instance->SetSite(webui_url);
  EXPECT_TRUE(instance->HasSite());

  // The call to GetProcess actually creates a new real process.
  host.reset(instance->GetProcess());
  EXPECT_TRUE(host.get() != nullptr);
  EXPECT_TRUE(instance->HasProcess());

  // Without bindings, this should look like the wrong process.
  EXPECT_TRUE(instance->HasWrongProcessForURL(webui_url));

  // WebUI uses process-per-site, so another instance would normally use the
  // same process.  Make sure it doesn't use the same process if the bindings
  // are missing.
  scoped_refptr<SiteInstanceImpl> instance2(
      SiteInstanceImpl::Create(browser_context.get()));
  instance2->SetSite(webui_url);
  host2.reset(instance2->GetProcess());
  EXPECT_TRUE(host2.get() != nullptr);
  EXPECT_TRUE(instance2->HasProcess());
  EXPECT_NE(host.get(), host2.get());

  DrainMessageLoop();
}

// Test that we do not register processes with empty sites for process-per-site
// mode.
TEST_F(SiteInstanceTest, NoProcessPerSiteForEmptySite) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerSite);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());
  std::unique_ptr<RenderProcessHost> host;
  scoped_refptr<SiteInstanceImpl> instance(
      SiteInstanceImpl::Create(browser_context.get()));

  instance->SetSite(GURL());
  EXPECT_TRUE(instance->HasSite());
  EXPECT_TRUE(instance->GetSiteURL().is_empty());
  host.reset(instance->GetProcess());

  EXPECT_FALSE(RenderProcessHostImpl::GetSoleProcessHostForURL(
      browser_context.get(), GURL()));
  EXPECT_FALSE(RenderProcessHostImpl::GetSoleProcessHostForSite(
      browser_context.get(), GURL(), GURL()));

  DrainMessageLoop();
}

// Check that an URL is considered same-site with blob: and filesystem: URLs
// with a matching inner origin.  See https://crbug.com/726370.
TEST_F(SiteInstanceTest, IsSameWebsiteForNestedURLs) {
  TestBrowserContext context;
  GURL foo_url("http://foo.com/");
  GURL bar_url("http://bar.com/");
  GURL blob_foo_url("blob:http://foo.com/uuid");
  GURL blob_bar_url("blob:http://bar.com/uuid");
  GURL fs_foo_url("filesystem:http://foo.com/path/");
  GURL fs_bar_url("filesystem:http://bar.com/path/");

  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, foo_url, blob_foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, blob_foo_url, foo_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, foo_url, blob_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, blob_foo_url, bar_url));

  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, foo_url, fs_foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, fs_foo_url, foo_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, foo_url, fs_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, fs_foo_url, bar_url));

  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, blob_foo_url, fs_foo_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, blob_foo_url, fs_bar_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, blob_foo_url, blob_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, fs_foo_url, fs_bar_url));

  // Verify that the scheme and ETLD+1 are used for comparison.
  GURL www_bar_url("http://www.bar.com/");
  GURL bar_org_url("http://bar.org/");
  GURL https_bar_url("https://bar.com/");
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, www_bar_url, bar_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, www_bar_url, blob_bar_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, www_bar_url, fs_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, bar_org_url, bar_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, bar_org_url, blob_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, bar_org_url, fs_bar_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, https_bar_url, bar_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, https_bar_url, blob_bar_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, https_bar_url, fs_bar_url));
}

TEST_F(SiteInstanceTest, IsolatedOrigins) {
  GURL foo_url("http://www.foo.com");
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL isolated_bar_url("http://isolated.bar.com");

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context;

  EXPECT_FALSE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_foo_url)));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, foo_url, isolated_foo_url));

  policy->AddIsolatedOrigins({url::Origin::Create(isolated_foo_url)});
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_foo_url)));
  EXPECT_FALSE(policy->IsIsolatedOrigin(url::Origin::Create(foo_url)));
  EXPECT_FALSE(
      policy->IsIsolatedOrigin(url::Origin::Create(GURL("http://foo.com"))));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://www.bar.com"))));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("https://isolated.foo.com"))));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://isolated.foo.com:12345"))));

  policy->AddIsolatedOrigins({url::Origin::Create(isolated_bar_url)});
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_bar_url)));

  // IsSameWebSite should compare origins rather than sites if either URL is an
  // isolated origin.
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, foo_url, isolated_foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, isolated_foo_url, foo_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, isolated_foo_url,
                                           isolated_bar_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, isolated_foo_url,
                                          isolated_foo_url));

  // Ensure blob and filesystem URLs with isolated origins are compared
  // correctly.
  GURL isolated_blob_foo_url("blob:http://isolated.foo.com/uuid");
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, isolated_foo_url,
                                          isolated_blob_foo_url));
  GURL isolated_filesystem_foo_url("filesystem:http://isolated.foo.com/bar/");
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, isolated_foo_url,
                                          isolated_filesystem_foo_url));

  // The site URL for an isolated origin should be the full origin rather than
  // eTLD+1.
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, isolated_foo_url));
  EXPECT_EQ(isolated_bar_url,
            SiteInstance::GetSiteForURL(&context, isolated_bar_url));
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, isolated_blob_foo_url));
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, isolated_filesystem_foo_url));

  // Isolated origins always require a dedicated process.
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, isolated_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, isolated_bar_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, isolated_blob_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, isolated_filesystem_foo_url));

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_foo_url));
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_bar_url));
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

  TestBrowserContext context;
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(isolated_url)});

  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(foo_isolated_url)));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://unisolated.com"))));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://isolated.foo.com"))));
  // Wrong scheme.
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("https://foo.isolated.com"))));

  // Appending a trailing dot to a URL should not bypass process isolation.
  EXPECT_TRUE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://isolated.com."))));
  EXPECT_TRUE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://foo.isolated.com."))));

  // A new SiteInstance created for a subdomain on an isolated origin
  // should use the isolated origin's host and not its own host as the site
  // URL.
  EXPECT_EQ(isolated_url,
            SiteInstance::GetSiteForURL(&context, foo_isolated_url));

  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, foo_isolated_url));

  EXPECT_TRUE(
      SiteInstance::IsSameWebSite(&context, isolated_url, foo_isolated_url));
  EXPECT_TRUE(
      SiteInstance::IsSameWebSite(&context, foo_isolated_url, isolated_url));

  // Don't try to match subdomains on IP addresses.
  GURL isolated_ip("http://127.0.0.1");
  policy->AddIsolatedOrigins({url::Origin::Create(isolated_ip)});
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_ip)));
  EXPECT_FALSE(policy->IsIsolatedOrigin(
      url::Origin::Create(GURL("http://42.127.0.0.1"))));

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_url));
}

TEST_F(SiteInstanceTest, SubdomainOnIsolatedOrigin) {
  GURL foo_url("http://foo.com");
  GURL isolated_foo_url("http://isolated.foo.com");
  GURL bar_isolated_foo_url("http://bar.isolated.foo.com");
  GURL baz_isolated_foo_url("http://baz.isolated.foo.com");

  TestBrowserContext context;
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(isolated_foo_url)});

  EXPECT_FALSE(policy->IsIsolatedOrigin(url::Origin::Create(foo_url)));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(isolated_foo_url)));
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin::Create(bar_isolated_foo_url)));
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin::Create(baz_isolated_foo_url)));

  EXPECT_EQ(foo_url, SiteInstance::GetSiteForURL(&context, foo_url));
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, isolated_foo_url));
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, bar_isolated_foo_url));
  EXPECT_EQ(isolated_foo_url,
            SiteInstance::GetSiteForURL(&context, baz_isolated_foo_url));

  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_FALSE(
        SiteInstanceImpl::DoesSiteRequireDedicatedProcess(&context, foo_url));
  }
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, isolated_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, bar_isolated_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, baz_isolated_foo_url));

  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, foo_url, isolated_foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, isolated_foo_url, foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, foo_url, bar_isolated_foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, bar_isolated_foo_url, foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, bar_isolated_foo_url,
                                          isolated_foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, isolated_foo_url,
                                          bar_isolated_foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, bar_isolated_foo_url,
                                          baz_isolated_foo_url));
  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, baz_isolated_foo_url,
                                          bar_isolated_foo_url));

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(isolated_foo_url));
}

TEST_F(SiteInstanceTest, MultipleIsolatedOriginsWithCommonSite) {
  GURL foo_url("http://foo.com");
  GURL bar_foo_url("http://bar.foo.com");
  GURL baz_bar_foo_url("http://baz.bar.foo.com");
  GURL qux_baz_bar_foo_url("http://qux.baz.bar.foo.com");

  TestBrowserContext context;
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins(
      {url::Origin::Create(foo_url), url::Origin::Create(baz_bar_foo_url)});

  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(foo_url)));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(bar_foo_url)));
  EXPECT_TRUE(policy->IsIsolatedOrigin(url::Origin::Create(baz_bar_foo_url)));
  EXPECT_TRUE(
      policy->IsIsolatedOrigin(url::Origin::Create(qux_baz_bar_foo_url)));

  EXPECT_EQ(foo_url, SiteInstance::GetSiteForURL(&context, foo_url));
  EXPECT_EQ(foo_url, SiteInstance::GetSiteForURL(&context, bar_foo_url));
  EXPECT_EQ(baz_bar_foo_url,
            SiteInstance::GetSiteForURL(&context, baz_bar_foo_url));
  EXPECT_EQ(baz_bar_foo_url,
            SiteInstance::GetSiteForURL(&context, qux_baz_bar_foo_url));

  EXPECT_TRUE(
      SiteInstanceImpl::DoesSiteRequireDedicatedProcess(&context, foo_url));
  EXPECT_TRUE(
      SiteInstanceImpl::DoesSiteRequireDedicatedProcess(&context, bar_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, baz_bar_foo_url));
  EXPECT_TRUE(SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
      &context, qux_baz_bar_foo_url));

  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, foo_url, bar_foo_url));
  EXPECT_FALSE(SiteInstance::IsSameWebSite(&context, foo_url, baz_bar_foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, foo_url, qux_baz_bar_foo_url));

  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, bar_foo_url, baz_bar_foo_url));
  EXPECT_FALSE(
      SiteInstance::IsSameWebSite(&context, bar_foo_url, qux_baz_bar_foo_url));

  EXPECT_TRUE(SiteInstance::IsSameWebSite(&context, baz_bar_foo_url,
                                          qux_baz_bar_foo_url));

  // Cleanup.
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(foo_url));
  policy->RemoveIsolatedOriginForTesting(url::Origin::Create(baz_bar_foo_url));
}

// Check that new SiteInstances correctly preserve the full URL that was used
// to initialize their site URL.
TEST_F(SiteInstanceTest, OriginalURL) {
  GURL original_url("https://foo.com/");
  GURL app_url("https://app.com/");
  EffectiveURLContentBrowserClient modified_client(original_url, app_url);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);
  std::unique_ptr<TestBrowserContext> browser_context(new TestBrowserContext());

  // The site URL of of effective URL should include both the effective URL's
  // site and the original URL's site.
  GURL expected_site_url(app_url.spec() + "#" + original_url.spec());

  // New SiteInstance in a new BrowsingInstance with a predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForURL(browser_context.get(), original_url);
    EXPECT_EQ(expected_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(original_url, site_instance->original_url());
  }

  // New related SiteInstance from an existing SiteInstance with a
  // predetermined URL.
  {
    scoped_refptr<SiteInstanceImpl> bar_site_instance =
        SiteInstanceImpl::CreateForURL(browser_context.get(),
                                       GURL("https://bar.com/"));
    scoped_refptr<SiteInstance> site_instance =
        bar_site_instance->GetRelatedSiteInstance(original_url);
    EXPECT_EQ(expected_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(
        original_url,
        static_cast<SiteInstanceImpl*>(site_instance.get())->original_url());
  }

  // New SiteInstance with a lazily assigned site URL.
  {
    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::Create(browser_context.get());
    EXPECT_FALSE(site_instance->HasSite());
    EXPECT_TRUE(site_instance->original_url().is_empty());
    site_instance->SetSite(original_url);
    EXPECT_EQ(expected_site_url, site_instance->GetSiteURL());
    EXPECT_EQ(original_url, site_instance->original_url());
  }

  SetBrowserClientForTesting(regular_client);
}

TEST_F(SiteInstanceTest, IsOriginLockASite) {
  EXPECT_FALSE(SiteInstanceImpl::IsOriginLockASite(GURL("http://")));
  EXPECT_FALSE(SiteInstanceImpl::IsOriginLockASite(GURL("")));
  EXPECT_FALSE(SiteInstanceImpl::IsOriginLockASite(GURL("google.com")));
  EXPECT_FALSE(SiteInstanceImpl::IsOriginLockASite(GURL("http:")));
  EXPECT_FALSE(SiteInstanceImpl::IsOriginLockASite(GURL("chrome:")));

  EXPECT_TRUE(SiteInstanceImpl::IsOriginLockASite(GURL("http://foo.com")));
  EXPECT_TRUE(SiteInstanceImpl::IsOriginLockASite(GURL("http://bar.foo.com")));
  EXPECT_TRUE(SiteInstanceImpl::IsOriginLockASite(
      GURL("http://user:pass@google.com:99/foo;bar?q=a#ref")));
}

}  // namespace content
