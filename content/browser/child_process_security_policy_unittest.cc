// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/storage_partition_test_helpers.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

const int kRendererID = 42;

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define TEST_PATH(x) FILE_PATH_LITERAL("c:") FILE_PATH_LITERAL(x)
#else
#define TEST_PATH(x) FILE_PATH_LITERAL(x)
#endif

class ChildProcessSecurityPolicyTestBrowserClient
    : public TestContentBrowserClient {
 public:
  ChildProcessSecurityPolicyTestBrowserClient() {}

  bool IsHandledURL(const GURL& url) override {
    return base::Contains(schemes_, url.scheme());
  }

  void ClearSchemes() {
    schemes_.clear();
  }

  void AddScheme(const std::string& scheme) {
    schemes_.insert(scheme);
  }

 private:
  std::set<std::string> schemes_;
};

void LockProcessIfNeeded(int process_id,
                         BrowserContext* browser_context,
                         const GURL& url) {
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForTesting(browser_context, url);
  if (site_instance->RequiresDedicatedProcess() &&
      site_instance->GetSiteInfo().ShouldLockProcessToSite(
          site_instance->GetIsolationContext())) {
    ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
        site_instance->GetIsolationContext(), process_id, false,
        ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()));
  }
}

}  // namespace

class ChildProcessSecurityPolicyTest : public testing::Test {
 public:
  ChildProcessSecurityPolicyTest()
      : task_environment_(BrowserTaskEnvironment::REAL_IO_THREAD),
        old_browser_client_(nullptr) {
  }

  void SetUp() override {
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);

    // Claim to always handle chrome:// URLs because the CPSP's notion of
    // allowing WebUI bindings is hard-wired to this particular scheme.
    test_browser_client_.AddScheme(kChromeUIScheme);

    // Claim to always handle file:// and android content:// URLs like the
    // browser would. net::URLRequest::IsHandledURL() no longer claims support
    // for default protocols as this is the responsibility of the browser (which
    // is responsible for adding the appropriate ProtocolHandler).
    test_browser_client_.AddScheme(url::kFileScheme);
#if BUILDFLAG(IS_ANDROID)
    test_browser_client_.AddScheme(url::kContentScheme);
#endif
    SiteIsolationPolicy::DisableFlagCachingForTesting();

    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->security_state_.size())
          << "ChildProcessSecurityPolicy should not be tracking any processes "
          << "at test startup.  Some other test probably forgot to call "
          << "Remove() at the end.";
    }
  }

  void TearDown() override {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->security_state_.size())
          << "ChildProcessSecurityPolicy should not be tracking any processes "
          << "at test shutdown.  Did you forget to call Remove() at the end of "
          << "a test?";
    }
    test_browser_client_.ClearSchemes();
    SetBrowserClientForTesting(old_browser_client_);
  }

  // Helpers to construct (key, value) entries used to validate the
  // isolated_origins_ map.  The key is a site URL, calculated from the
  // provided origin, and the value is a list of IsolatedOriginEntries. These
  // helpers are members of ChildProcessSecurityPolicyTest so they can access
  // the private IsolatedOriginEntry struct.
  using IsolatedOriginEntry =
      ChildProcessSecurityPolicyImpl::IsolatedOriginEntry;
  // Converts |browsing_instance_id|, |origin| -> (site_url, {entry}) where
  // site_url is created from |origin|, and {entry} contains |origin|
  // and |browsing_instance_id|.
  auto GetIsolatedOriginEntry(BrowsingInstanceId browsing_instance_id,
                              const url::Origin& origin,
                              bool isolate_all_subdomains = false) {
    return std::pair<GURL, std::vector<IsolatedOriginEntry>>(
        SiteInfo::GetSiteForOrigin(origin),
        {IsolatedOriginEntry(
            origin, true /* applies_to_future_browsing_instances */,
            browsing_instance_id, nullptr, nullptr, isolate_all_subdomains,
            IsolatedOriginSource::TEST)});
  }
  auto GetIsolatedOriginEntry(int browsing_instance_id,
                              const url::Origin& origin,
                              bool isolate_all_subdomains = false) {
    return GetIsolatedOriginEntry(BrowsingInstanceId(browsing_instance_id),
                                  origin, isolate_all_subdomains);
  }
  // Converts the provided params into a (site_url, {entry}) tuple, where
  // site_url is created from |origin| and {entry} contains |origin| and
  // matches the provided BrowserContext, BrowsingInstance ID, and whether the
  // isolation applies to future BrowsingInstances.
  auto GetIsolatedOriginEntry(BrowserContext* browser_context,
                              bool applies_to_future_browsing_instances,
                              BrowsingInstanceId browsing_instance_id,
                              const url::Origin& origin) {
    return std::pair<GURL, std::vector<IsolatedOriginEntry>>(
        SiteInfo::GetSiteForOrigin(origin),
        {IsolatedOriginEntry(
            origin, applies_to_future_browsing_instances, browsing_instance_id,
            browser_context,
            browser_context ? browser_context->GetResourceContext() : nullptr,
            false /* isolate_all_subdomains */, IsolatedOriginSource::TEST)});
  }
  // Converts |origin| -> (site_url, {entry})
  //     where site_url is created from |origin| and
  //           entry contains |origin| and the latest BrowsingInstance ID.
  auto GetIsolatedOriginEntry(const url::Origin& origin,
                              bool isolate_all_subdomains = false) {
    return GetIsolatedOriginEntry(SiteInstanceImpl::NextBrowsingInstanceId(),
                                  origin, isolate_all_subdomains);
  }
  // Converts |origin1|, |origin2| -> (site_url, {entry1, entry2})
  //     where |site_url| is created from |origin1|, but is assumed to be the
  //               same for |origin2| (i.e., |origin1| and |origin2| are
  //               same-site),
  //           entry1 contains |origin1| and the latest BrowsingInstance ID,
  //           entry2 contains |origin2| and the latest BrowsingInstance ID.
  auto GetIsolatedOriginEntry(const url::Origin& origin1,
                              const url::Origin& origin2,
                              bool origin1_isolate_all_subdomains = false,
                              bool origin2_isolate_all_subdomains = false) {
    EXPECT_EQ(SiteInfo::GetSiteForOrigin(origin1),
              SiteInfo::GetSiteForOrigin(origin2));
    return std::pair<GURL, std::vector<IsolatedOriginEntry>>(
        SiteInfo::GetSiteForOrigin(origin1),
        {IsolatedOriginEntry(
             origin1, true /* applies_to_future_browsing_contexts */,
             SiteInstanceImpl::NextBrowsingInstanceId(), nullptr, nullptr,
             origin1_isolate_all_subdomains, IsolatedOriginSource::TEST),
         IsolatedOriginEntry(
             origin2, true /* applies_to_future_browsing_contexts */,
             SiteInstanceImpl::NextBrowsingInstanceId(), nullptr, nullptr,
             origin2_isolate_all_subdomains, IsolatedOriginSource::TEST)});
  }

  bool IsIsolatedOrigin(BrowserContext* context,
                        int browsing_instance_id,
                        const url::Origin& origin) {
    return IsIsolatedOrigin(context, BrowsingInstanceId(browsing_instance_id),
                            origin);
  }

  bool IsIsolatedOrigin(BrowserContext* context,
                        BrowsingInstanceId browsing_instance_id,
                        const url::Origin& origin) {
    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    return p->IsIsolatedOrigin(
        IsolationContext(
            browsing_instance_id, context,
            /*is_guest=*/false, /*is_fenced=*/false,
            OriginAgentClusterIsolationState::CreateForDefaultIsolation(
                &browser_context_)),
        origin, false /* origin_requests_isolation */);
  }

  // Returns the number of isolated origin entries for a particular origin.
  // There may be more than one such entry if each is associated with a
  // different profile.
  int GetIsolatedOriginEntryCount(const url::Origin& origin) {
    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    GURL key(SiteInfo::GetSiteForOrigin(origin));
    base::AutoLock isolated_origins_lock(p->isolated_origins_lock_);
    auto origins_for_key = p->isolated_origins_[key];
    return base::ranges::count(origins_for_key, origin,
                               &IsolatedOriginEntry::origin);
  }

  void CheckGetSiteForURL(BrowserContext* context,
                          std::map<GURL, GURL> to_test) {
    for (const auto& entry : to_test) {
      auto site_info =
          SiteInfo::CreateForTesting(IsolationContext(context), entry.first);
      EXPECT_EQ(site_info.site_url(), entry.second);
    }
  }

 protected:
  void RegisterTestScheme(const std::string& scheme) {
    test_browser_client_.AddScheme(scheme);
  }

  void GrantPermissionsForFile(ChildProcessSecurityPolicyImpl* p,
                               int child_id,
                               const base::FilePath& file,
                               int permissions) {
    p->GrantPermissionsForFile(child_id, file, permissions);
  }

  void CheckHasNoFileSystemPermission(ChildProcessSecurityPolicyImpl* p,
                                      const std::string& child_id) {
    EXPECT_FALSE(p->CanReadFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, child_id));
    EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, child_id));
  }

  void CheckHasNoFileSystemFilePermission(ChildProcessSecurityPolicyImpl* p,
                                          const base::FilePath& file,
                                          const storage::FileSystemURL& url) {
    EXPECT_FALSE(p->CanReadFile(kRendererID, file));
    EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, file));
    EXPECT_FALSE(p->CanReadFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, url));
    EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, url));

    auto handle = p->CreateHandle(kRendererID);
    EXPECT_FALSE(handle.CanReadFile(file));
    EXPECT_FALSE(handle.CanReadFileSystemFile(url));
  }

  BrowserContext* browser_context() { return &browser_context_; }

 private:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  ChildProcessSecurityPolicyTestBrowserClient test_browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

// A test class that forces kOriginKeyedProcessesByDefault off in
// ChildProcessSecurityPolicyTest. Used for tests that are trying to verify
// behavior that is inconsistent with Origin Isolation.
class ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault
    : public ChildProcessSecurityPolicyTest {
 public:
  ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault() {
    feature_list_.InitAndDisableFeature(
        features::kOriginKeyedProcessesByDefault);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ChildProcessSecurityPolicyTest, ChildID) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->AddForTesting(kRendererID, browser_context());
  auto handle = p->CreateHandle(kRendererID);
  EXPECT_EQ(handle.child_id(), kRendererID);
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, IsWebSafeSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_TRUE(p->IsWebSafeScheme(url::kHttpScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kHttpsScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kDataScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kBlobScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kFileSystemScheme));

  EXPECT_FALSE(p->IsWebSafeScheme("registered-web-safe-scheme"));
  p->RegisterWebSafeScheme("registered-web-safe-scheme");
  EXPECT_TRUE(p->IsWebSafeScheme("registered-web-safe-scheme"));

  EXPECT_FALSE(p->IsWebSafeScheme(kChromeUIScheme));

  p->ClearRegisteredSchemeForTesting("registered-web-safe-scheme");
}

TEST_F(ChildProcessSecurityPolicyTest, IsPseudoSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  EXPECT_TRUE(p->IsPseudoScheme(url::kAboutScheme));
  EXPECT_TRUE(p->IsPseudoScheme(url::kJavaScriptScheme));
  EXPECT_TRUE(p->IsPseudoScheme(kViewSourceScheme));
  EXPECT_TRUE(p->IsPseudoScheme(kGoogleChromeScheme));

  EXPECT_FALSE(p->IsPseudoScheme("registered-pseudo-scheme"));
  p->RegisterPseudoScheme("registered-pseudo-scheme");
  EXPECT_TRUE(p->IsPseudoScheme("registered-pseudo-scheme"));

  EXPECT_FALSE(p->IsPseudoScheme(kChromeUIScheme));

  p->ClearRegisteredSchemeForTesting("registered-pseudo-scheme");
}

TEST_F(ChildProcessSecurityPolicyTest, StandardSchemesTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  auto handle = p->CreateHandle(kRendererID);

  // Safe to request, redirect or commit.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanRequestURL(
      kRendererID, GURL("filesystem:http://localhost/temporary/a.gif")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("filesystem:http://localhost/temporary/a.gif")));

  const std::vector<std::string> kCommitURLs({
      "http://www.google.com/",
      "https://www.paypal.com/",
      "filesystem:http://localhost/temporary/a.gif",
  });
  for (const auto& url_string : kCommitURLs) {
    const GURL commit_url(url_string);
    if (AreAllSitesIsolatedForTesting()) {
      // A non-locked process cannot access URL (because with
      // site-per-process all the URLs need to be isolated).
      EXPECT_FALSE(p->CanCommitURL(kRendererID, commit_url)) << commit_url;
    } else {
      EXPECT_TRUE(p->CanCommitURL(kRendererID, commit_url)) << commit_url;
    }
  }

  // A data URL can commit in any process.
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("data:text/html,<b>Hi</b>")));

  // Dangerous to request, commit, or set as origin header.
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GetWebUIURL("foo/bar")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:http://www.google.com/")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GetWebUIURL("foo/bar")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL(kUnreachableWebDataURL)));

  const std::vector<std::string> kFailedCommitURLs(
      {"file:///etc/passwd", "view-source:http://www.google.com/",
       kUnreachableWebDataURL, GetWebUIURL("foo/bar").spec()});
  for (const auto& url_string : kFailedCommitURLs) {
    const GURL commit_url(url_string);
    EXPECT_FALSE(p->CanCommitURL(kRendererID, commit_url)) << commit_url;
  }

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, BlobSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL localhost_url("http://localhost/");
  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), localhost_url);

  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("blob:NulL/some-guid?query")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("filesystem:blob:http://localhost/guid")));

  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:null/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:NulL/some-guid?query")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("blob:http://username@localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(
      GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:blob:some-guid")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("blob:some-guid")));
  EXPECT_TRUE(
      p->CanRedirectToURL(GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(
      p->CanRedirectToURL(GURL("filesystem:blob:http://localhost/guid")));

  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(kRendererID, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("filesystem:blob:http://localhost/guid")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, AboutTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:blank")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("about:blank")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("about:blank")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("aBouT:blank")));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:srcdoc")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:SRCDOC")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:SRCDOC")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:hang")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:cache")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:hang")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:version")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:cache")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:hang")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:version")));

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("aBoUt:version")));

  // Requests for about: pages should be denied.
  p->GrantCommitURL(kRendererID, GURL("about:crash"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("about:crash")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, JavaScriptTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("javascript:alert('xss')")));
  p->GrantCommitURL(kRendererID, GURL("javascript:alert('xss')"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("javascript:alert('xss')")));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, RegisterWebSafeSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  // Currently, "asdf" is destined for ShellExecute, so it is allowed to be
  // requested but not committed.
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));

  // Once we register "asdf", we default to deny.
  RegisterTestScheme("asdf");
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));

  // We can allow new schemes by adding them to the whitelist.
  p->RegisterWebSafeScheme("asdf");
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  if (AreAllSitesIsolatedForTesting()) {
    // With site-per-process, all URLs (including the one below) will ask to be
    // hosted in isolated processes.  Since |p| is not locked, CanCommitURL
    // should return false.
    EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));

    // After locking the process, CanCommitURL should start returning true.
    LockProcessIfNeeded(kRendererID, browser_context(), GURL("asdf:rockers"));
    EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));
  } else {
    EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("asdf:rockers")));
  }

  // Cleanup.
  p->Remove(kRendererID);
  p->ClearRegisteredSchemeForTesting("asdf");
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceCommandsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL file_url("file:///etc/passwd");
  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), file_url);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  p->GrantCommitURL(kRendererID, GURL("file:///etc/passwd"));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));

  // We should forget our state if we repeat a renderer id.
  p->Remove(kRendererID);
  p->AddForTesting(kRendererID, browser_context());
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, ViewSource) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  // Child processes cannot request view source URLs.
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID,
                                GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  // Child processes cannot be redirected to view source URLs.
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(
      GURL("view-source:view-source:http://www.google.com/")));

  // View source URLs don't actually commit; the renderer is put into view
  // source mode, and the inner URL commits.
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(
      kRendererID, GURL("view-source:view-source:http://www.google.com/")));

  p->GrantCommitURL(kRendererID, GURL("view-source:file:///etc/passwd"));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanRequestURL(kRendererID, GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(kRendererID,
                               GURL("view-source:file:///etc/passwd")));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, GoogleChromeScheme) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());

  GURL test_url("googlechrome://whatever");

  EXPECT_FALSE(p->CanRequestURL(kRendererID, test_url));
  EXPECT_FALSE(p->CanRedirectToURL(test_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, test_url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, GrantCommitURLToNonStandardScheme) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const GURL url("httpxml://awesome");
  const GURL url2("httpxml://also-awesome");

  ASSERT_TRUE(url::Origin::Create(url).opaque());
  ASSERT_TRUE(url::Origin::Create(url2).opaque());
  RegisterTestScheme("httpxml");

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), url);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url2));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanRedirectToURL(url2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url2));

  // GrantCommitURL with a non-standard scheme should grant commit access to the
  // entire scheme.
  p->GrantCommitURL(kRendererID, url);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url2));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanRedirectToURL(url2));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url2));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, SpecificFile) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL icon_url("file:///tmp/foo.png");
  base::FilePath icon_path;
  ASSERT_TRUE(net::FileURLToFilePath(icon_url, &icon_path));
  GURL sensitive_url("file:///etc/passwd");

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), sensitive_url);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, sensitive_url));

  p->GrantRequestOfSpecificFile(kRendererID, icon_path);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, sensitive_url));

  p->GrantCommitURL(kRendererID, icon_url);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, icon_url));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, icon_url));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, sensitive_url));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, ContentUri) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL content_uri("content://authority/foo.png");
  GURL content_uri_sensitive("content://authority/bar.jpg");

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), content_uri_sensitive);

#if BUILDFLAG(IS_ANDROID)
  // Since android handles content:// URLs, CanRequestURL() is false for a URL
  // which was not registered with GrantRequestOfSpecificFile().
  EXPECT_FALSE(p->CanRequestURL(kRendererID, content_uri));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, content_uri_sensitive));
#else
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri_sensitive));
#endif
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, content_uri));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, content_uri_sensitive));

  p->GrantRequestOfSpecificFile(
      kRendererID,
      base::FilePath::FromUTF8Unsafe(content_uri.possibly_invalid_spec()));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(p->CanRequestURL(kRendererID, content_uri_sensitive));
#else
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri_sensitive));
#endif
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, content_uri));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, content_uri_sensitive));

  p->GrantCommitURL(kRendererID, content_uri);
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, content_uri_sensitive));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, content_uri));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, content_uri_sensitive));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, FileSystemGrantsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->AddForTesting(kRendererID, browser_context());
  std::string read_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "read_filesystem", base::FilePath());
  std::string read_write_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "read_write_filesystem",
          base::FilePath());
  std::string copy_into_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "copy_into_filesystem",
          base::FilePath());
  std::string delete_from_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest,
          "delete_from_filesystem",
          base::FilePath());

  // Test initially having no permissions.
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Testing varying combinations of grants and checks.
  p->GrantReadFileSystem(kRendererID, read_id);
  EXPECT_TRUE(p->CanReadFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, read_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, read_id));

  p->GrantReadFileSystem(kRendererID, read_write_id);
  p->GrantWriteFileSystem(kRendererID, read_write_id);
  EXPECT_TRUE(p->CanReadFileSystem(kRendererID, read_write_id));
  EXPECT_TRUE(p->CanReadWriteFileSystem(kRendererID, read_write_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, read_write_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, read_write_id));

  p->GrantCopyIntoFileSystem(kRendererID, copy_into_id);
  EXPECT_FALSE(p->CanReadFileSystem(kRendererID, copy_into_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, copy_into_id));
  EXPECT_TRUE(p->CanCopyIntoFileSystem(kRendererID, copy_into_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(kRendererID, copy_into_id));

  p->GrantDeleteFromFileSystem(kRendererID, delete_from_id);
  EXPECT_FALSE(p->CanReadFileSystem(kRendererID, delete_from_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(kRendererID, delete_from_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(kRendererID, delete_from_id));
  EXPECT_TRUE(p->CanDeleteFromFileSystem(kRendererID, delete_from_id));

  // Test revoke permissions on renderer ID removal.
  p->Remove(kRendererID);
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Test having no permissions upon re-adding same renderer ID.
  p->AddForTesting(kRendererID, browser_context());
  CheckHasNoFileSystemPermission(p, read_id);
  CheckHasNoFileSystemPermission(p, read_write_id);
  CheckHasNoFileSystemPermission(p, copy_into_id);
  CheckHasNoFileSystemPermission(p, delete_from_id);

  // Cleanup.
  p->Remove(kRendererID);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_write_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(copy_into_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(delete_from_id);
}

TEST_F(ChildProcessSecurityPolicyTest, FilePermissionGrantingAndRevoking) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->RegisterFileSystemPermissionPolicy(
      storage::kFileSystemTypeTest,
      storage::FILE_PERMISSION_USE_FILE_PERMISSION);

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), GURL("http://foo/"));

  base::FilePath file(TEST_PATH("/dir/testfile"));
  file = file.NormalizePathSeparators();
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo/"),
      storage::kFileSystemTypeTest, file);

  // Test initially having no permissions.
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Testing every combination of permissions granting and revoking.
  p->GrantReadFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_FALSE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->RevokeAllPermissionsForFile(kRendererID, file);
  CheckHasNoFileSystemFilePermission(p, file, url);

  p->GrantCreateReadWriteFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->RevokeAllPermissionsForFile(kRendererID, file);
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Test revoke permissions on renderer ID removal.
  p->GrantCreateReadWriteFile(kRendererID, file);
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(kRendererID, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(kRendererID, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(kRendererID, url));
  p->Remove(kRendererID);
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Test having no permissions upon re-adding same renderer ID.
  p->AddForTesting(kRendererID, browser_context());
  CheckHasNoFileSystemFilePermission(p, file, url);
  LockProcessIfNeeded(kRendererID, browser_context(), GURL("http://foo/"));
  CheckHasNoFileSystemFilePermission(p, file, url);

  // Cleanup.
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, FilePermissions) {
  base::FilePath granted_file = base::FilePath(TEST_PATH("/home/joe"));
  base::FilePath sibling_file = base::FilePath(TEST_PATH("/home/bob"));
  base::FilePath child_file = base::FilePath(TEST_PATH("/home/joe/file"));
  base::FilePath parent_file = base::FilePath(TEST_PATH("/home"));
  base::FilePath parent_slash_file = base::FilePath(TEST_PATH("/home/"));
  base::FilePath child_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/././file"));
  base::FilePath child_traversal2 = base::FilePath(
      TEST_PATH("/home/joe/file/../otherfile"));
  base::FilePath evil_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/../../etc/passwd"));
  base::FilePath evil_traversal2 = base::FilePath(
      TEST_PATH("/home/joe/./.././../etc/passwd"));
  base::FilePath self_traversal =
      base::FilePath(TEST_PATH("/home/joe/../joe/file"));
  base::FilePath relative_file = base::FilePath(FILE_PATH_LITERAL("home/joe"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Grant permissions for a file.
  p->AddForTesting(kRendererID, browser_context());
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));

  GrantPermissionsForFile(p, kRendererID, granted_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_OPEN_TRUNCATED |
                             base::File::FLAG_READ |
                             base::File::FLAG_WRITE);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_OPEN_TRUNCATED |
                                       base::File::FLAG_READ |
                                       base::File::FLAG_WRITE));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_CREATE));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file, 0));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_CREATE |
                                        base::File::FLAG_OPEN_TRUNCATED |
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, sibling_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, parent_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_file,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_traversal1,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, child_traversal2,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, evil_traversal1,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, evil_traversal2,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  // CPSP doesn't allow this case for the sake of simplicity.
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, self_traversal,
                                        base::File::FLAG_OPEN |
                                        base::File::FLAG_READ));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in.
  p->AddForTesting(kRendererID, browser_context());
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, kRendererID, parent_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));
  p->Remove(kRendererID);

  // Grant permissions for the directory the file is in (with trailing '/').
  p->AddForTesting(kRendererID, browser_context());
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, kRendererID, parent_slash_file,
                             base::File::FLAG_OPEN |
                             base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_READ |
                                        base::File::FLAG_WRITE));

  // Grant permissions for the file (should overwrite the permissions granted
  // for the directory).
  GrantPermissionsForFile(p, kRendererID, granted_file,
                          base::File::FLAG_WIN_TEMPORARY);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_WIN_TEMPORARY));

  // Revoke all permissions for the file (it should inherit its permissions
  // from the directory again).
  p->RevokeAllPermissionsForFile(kRendererID, granted_file);
  EXPECT_TRUE(p->HasPermissionsForFile(kRendererID, granted_file,
                                       base::File::FLAG_OPEN |
                                       base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, granted_file,
                                        base::File::FLAG_WIN_TEMPORARY));
  p->Remove(kRendererID);

  p->AddForTesting(kRendererID, browser_context());
  GrantPermissionsForFile(p, kRendererID, relative_file,
                             base::File::FLAG_OPEN);
  EXPECT_FALSE(p->HasPermissionsForFile(kRendererID, relative_file,
                                        base::File::FLAG_OPEN));
  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, CanServiceWebUIBindings) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const GURL url(GetWebUIURL("thumb/http://www.google.com/"));
  const GURL other_url(GetWebUIURL("not-thumb/"));
  const url::Origin origin = url::Origin::Create(url);
  {
    p->AddForTesting(kRendererID, browser_context());
    LockProcessIfNeeded(kRendererID, browser_context(), url);

    EXPECT_FALSE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantWebUIBindings(kRendererID,
                          BindingsPolicySet({BindingsPolicyValue::kWebUi}));

    EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantCommitOrigin(kRendererID, origin);

    EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
    EXPECT_TRUE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->Remove(kRendererID);
  }

  {
    p->AddForTesting(kRendererID, browser_context());
    LockProcessIfNeeded(kRendererID, browser_context(), url);

    EXPECT_FALSE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantWebUIBindings(kRendererID,
                          BindingsPolicySet({BindingsPolicyValue::kMojoWebUi}));

    EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantCommitOrigin(kRendererID, origin);

    EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
    EXPECT_TRUE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->Remove(kRendererID);
  }

  {
    p->AddForTesting(kRendererID, browser_context());
    LockProcessIfNeeded(kRendererID, browser_context(), url);

    EXPECT_FALSE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantWebUIBindings(kRendererID, kWebUIBindingsPolicySet);

    EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->GrantCommitOrigin(kRendererID, origin);

    EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
    EXPECT_TRUE(p->CanCommitURL(kRendererID, url));
    EXPECT_TRUE(p->CanRedirectToURL(url));

    EXPECT_FALSE(p->CanRequestURL(kRendererID, other_url));
    EXPECT_FALSE(p->CanCommitURL(kRendererID, other_url));
    EXPECT_TRUE(p->CanRedirectToURL(other_url));

    p->Remove(kRendererID);
  }
}

TEST_F(ChildProcessSecurityPolicyTest, RemoveRace) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("file:///etc/passwd");
  base::FilePath file(TEST_PATH("/etc/passwd"));

  p->AddForTesting(kRendererID, browser_context());

  p->GrantCommitURL(kRendererID, url);
  p->GrantReadFile(kRendererID, file);
  p->GrantWebUIBindings(kRendererID, kWebUIBindingsPolicySet);

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanReadFile(kRendererID, file));
  EXPECT_TRUE(p->HasWebUIBindings(kRendererID));

  p->Remove(kRendererID);

  // Renderers are added and removed on the UI thread, but the policy can be
  // queried on the IO thread.  The ChildProcessSecurityPolicy needs to be
  // prepared to answer policy questions about renderers who no longer exist.

  // In this case, we default to secure behavior.
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_FALSE(p->CanReadFile(kRendererID, file));
  EXPECT_FALSE(p->HasWebUIBindings(kRendererID));
}

TEST_F(ChildProcessSecurityPolicyTest, HandleDuplicate) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("file:///etc/passwd");

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), url);

  auto handle = p->CreateHandle(kRendererID);

  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(url)));

  // Verify that a valid duplicate can be created and allows access.
  auto duplicate_handle = handle.Duplicate();
  EXPECT_TRUE(duplicate_handle.is_valid());
  EXPECT_TRUE(
      duplicate_handle.CanAccessDataForOrigin(url::Origin::Create(url)));

  p->Remove(kRendererID);

  // Verify that both handles still work even after Remove() has been called.
  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(url)));
  EXPECT_TRUE(
      duplicate_handle.CanAccessDataForOrigin(url::Origin::Create(url)));

  // Verify that a new duplicate can be created after Remove().
  auto duplicate_handle2 = handle.Duplicate();
  EXPECT_TRUE(duplicate_handle2.is_valid());
  EXPECT_TRUE(
      duplicate_handle2.CanAccessDataForOrigin(url::Origin::Create(url)));

  // Verify that a new valid Handle cannot be created after Remove().
  EXPECT_FALSE(p->CreateHandle(kRendererID).is_valid());

  // Invalidate the original Handle and verify that the duplicates still work.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)));
  EXPECT_TRUE(
      duplicate_handle.CanAccessDataForOrigin(url::Origin::Create(url)));
  EXPECT_TRUE(
      duplicate_handle2.CanAccessDataForOrigin(url::Origin::Create(url)));
}

TEST_F(ChildProcessSecurityPolicyTest, CanAccessDataForOrigin_URL) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL file_url("file:///etc/passwd");
  GURL foo_http_url("http://foo.com/index.html");
  GURL foo_blob_url("blob:http://foo.com/43d75119-d7af-4471-a293-07c6b3d7e61a");
  GURL foo_filesystem_url("filesystem:http://foo.com/temporary/test.html");
  GURL bar_http_url("http://bar.com/index.html");

  const std::vector<GURL> kAllTestUrls = {file_url, foo_http_url, foo_blob_url,
                                          foo_filesystem_url, bar_http_url};

  // Test invalid ID and invalid Handle cases.
  auto handle = p->CreateHandle(kRendererID);
  for (auto url : kAllTestUrls) {
    EXPECT_FALSE(
        p->CanAccessDataForOrigin(kRendererID, url::Origin::Create(url)))
        << url;
    EXPECT_FALSE(
        handle.CanAccessDataForOrigin(url::Origin::Create(bar_http_url)))
        << url;
  }

  TestBrowserContext browser_context;
  p->AddForTesting(kRendererID, &browser_context);

  // Replace the old invalid handle with a new valid handle.
  handle = p->CreateHandle(kRendererID);

  // Verify unlocked origin permissions.
  for (auto url : kAllTestUrls) {
    if (AreAllSitesIsolatedForTesting()) {
      // A non-locked process cannot access URLs below (because with
      // site-per-process all the URLs need to be isolated).
      EXPECT_FALSE(
          p->CanAccessDataForOrigin(kRendererID, url::Origin::Create(url)))
          << url;
      EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)))
          << url;
    } else {
      EXPECT_TRUE(
          p->CanAccessDataForOrigin(kRendererID, url::Origin::Create(url)))
          << url;
      EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(url)))
          << url;
    }
  }

  // Isolate |http_url| so we can't get a default SiteInstance.
  p->AddFutureIsolatedOrigins({url::Origin::Create(foo_http_url)},
                              IsolatedOriginSource::TEST, &browser_context);

  // Lock process to |http_url| origin.
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&browser_context, foo_http_url);
  EXPECT_FALSE(foo_instance->IsDefaultSiteInstance());
  LockProcessIfNeeded(kRendererID, &browser_context, foo_http_url);

  // Verify that file access is no longer allowed.
  EXPECT_FALSE(
      p->CanAccessDataForOrigin(kRendererID, url::Origin::Create(file_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID,
                                        url::Origin::Create(foo_http_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID,
                                        url::Origin::Create(foo_blob_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(
      kRendererID, url::Origin::Create(foo_filesystem_url)));
  EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID,
                                         url::Origin::Create(bar_http_url)));
  EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(file_url)));
  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(foo_http_url)));
  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(foo_blob_url)));
  EXPECT_TRUE(
      handle.CanAccessDataForOrigin(url::Origin::Create(foo_filesystem_url)));
  EXPECT_FALSE(
      handle.CanAccessDataForOrigin(url::Origin::Create(bar_http_url)));

  // Invalidate handle so it does not preserve security state beyond Remove().
  handle = ChildProcessSecurityPolicyImpl::Handle();

  p->Remove(kRendererID);

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify invalid ID is rejected now that Remove() has completed.
  for (auto url : kAllTestUrls) {
    EXPECT_FALSE(
        p->CanAccessDataForOrigin(kRendererID, url::Origin::Create(url)))
        << url;
    EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)))
        << url;
  }
}

TEST_F(ChildProcessSecurityPolicyTest, CanAccessDataForOrigin_Origin) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const std::vector<const char*> foo_urls = {
      "http://foo.com/index.html",
      "blob:http://foo.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://foo.com/temporary/test.html",
      // Port differences considered equal.
      "http://foo.com:1234/index.html",
      "blob:http://foo.com:1234/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://foo.com:1234/temporary/test.html",
      // TODO(acolwell): data: should be in |non_foo_urls| in the long-term.
      "data:text/html,Hello!"};

  const std::vector<const char*> non_foo_urls = {
      "file:///etc/passwd",
      "http://bar.com/index.html",
      "blob:http://bar.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://bar.com/temporary/test.html",
      // foo.com with a different scheme not considered equal.
      "https://foo.com/index.html",
      "blob:https://foo.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:https://foo.com/temporary/test.html"};

  std::vector<url::Origin> foo_origins;
  std::vector<url::Origin> non_foo_origins;
  std::vector<url::Origin> all_origins;
  for (auto* url : foo_urls) {
    auto origin = url::Origin::Create(GURL(url));
    foo_origins.push_back(origin);
    all_origins.push_back(origin);
  }
  auto foo_origin = url::Origin::Create(GURL("http://foo.com"));
  auto opaque_with_foo_precursor = foo_origin.DeriveNewOpaqueOrigin();
  foo_origins.push_back(opaque_with_foo_precursor);
  all_origins.push_back(opaque_with_foo_precursor);

  for (auto* url : non_foo_urls) {
    auto origin = url::Origin::Create(GURL(url));
    non_foo_origins.push_back(origin);
    all_origins.push_back(origin);
  }
  url::Origin opaque_origin_without_precursor;
  // TODO(acolwell): This should be in |non_foo_origins| in the long-term.
  foo_origins.push_back(opaque_origin_without_precursor);
  all_origins.push_back(opaque_origin_without_precursor);

  auto opaque_with_bar_precursor =
      url::Origin::Create(GURL("http://bar.com")).DeriveNewOpaqueOrigin();
  non_foo_origins.push_back(opaque_with_bar_precursor);
  all_origins.push_back(opaque_with_bar_precursor);

  // Test invalid process ID for all cases.
  for (const auto& origin : all_origins)
    EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;

  TestBrowserContext browser_context;
  p->AddForTesting(kRendererID, &browser_context);

  // Verify unlocked process permissions.
  for (const auto& origin : all_origins) {
    if (AreAllSitesIsolatedForTesting()) {
      if (origin.opaque() &&
          !origin.GetTupleOrPrecursorTupleIfOpaque().IsValid()) {
        EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;
      } else {
        EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;
      }
    } else {
      EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;
    }
  }

  // Isolate |foo_origin| so we can't get a default SiteInstance.
  p->AddFutureIsolatedOrigins({foo_origin}, IsolatedOriginSource::TEST,
                              &browser_context);

  // Lock process to |foo_origin| origin.
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&browser_context, foo_origin.GetURL());
  EXPECT_FALSE(foo_instance->IsDefaultSiteInstance());
  LockProcessIfNeeded(kRendererID, &browser_context, foo_origin.GetURL());

  // Verify that access is no longer allowed for origins that are not associated
  // with foo.com.
  for (const auto& origin : foo_origins)
    EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;

  for (const auto& origin : non_foo_origins)
    EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;

  p->Remove(kRendererID);

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify invalid ID is rejected now that Remove() has completed.
  for (const auto& origin : all_origins)
    EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID, origin)) << origin;
}

TEST_F(ChildProcessSecurityPolicyTest, SandboxedProcessEnforcements) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext browser_context;
  p->AddForTesting(kRendererID, &browser_context);

  // Create a ProcessLock for a process-isolated sandboxed frame, and lock the
  // kRendererID process to it.
  UrlInfo sandboxed_url_info(
      UrlInfoInit(GURL("https://foo.com")).WithSandbox(true));
  scoped_refptr<SiteInstanceImpl> sandboxed_instance =
      SiteInstanceImpl::CreateForUrlInfo(&browser_context, sandboxed_url_info,
                                         /*is_guest=*/false,
                                         /*is_fenced=*/false,
                                         /*is_fixed_storage_partition=*/false);
  p->LockProcess(sandboxed_instance->GetIsolationContext(), kRendererID,
                 /*is_process_used=*/false,
                 ProcessLock::FromSiteInfo(sandboxed_instance->GetSiteInfo()));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  auto opaque_foo_origin = foo_origin.DeriveNewOpaqueOrigin();
  auto bar_origin = url::Origin::Create(GURL("https://bar.com"));
  auto opaque_bar_origin = bar_origin.DeriveNewOpaqueOrigin();

  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  // A sandboxed process should be able to commit new URLs, as long as they
  // have an opaque origin with a matching precursor.
  EXPECT_TRUE(p->CanAccessOrigin(kRendererID, opaque_foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  // TODO(crbug.com/325410297): Currently, non-opaque origins are allowed to
  // commit. Fix this and flip the expectation to false.
  EXPECT_TRUE(p->CanAccessOrigin(kRendererID, foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(kRendererID, bar_origin,
                                  AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(kRendererID, opaque_bar_origin,
                                  AccessType::kCanCommitNewOrigin));

  // A sandboxed process should not be able to access data for any origin.
  EXPECT_FALSE(
      p->CanAccessOrigin(kRendererID, opaque_foo_origin,
                         AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      kRendererID, foo_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      kRendererID, bar_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(kRendererID, opaque_bar_origin,
                         AccessType::kCanAccessDataForCommittedOrigin));

  // A sandboxed process should only be able to claim that it has an opaque
  // origin.
  EXPECT_TRUE(p->CanAccessOrigin(kRendererID, opaque_foo_origin,
                                 AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(kRendererID, foo_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(kRendererID, bar_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(kRendererID, opaque_bar_origin,
                                  AccessType::kHostsOrigin));

  p->Remove(kRendererID);
}

TEST_F(ChildProcessSecurityPolicyTest, PdfProcessEnforcements) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext browser_context;
  p->AddForTesting(kRendererID, &browser_context);

  // Create a ProcessLock for a PDF renderer, and lock the kRendererID process
  // to it.
  UrlInfo pdf_url_info(UrlInfoInit(GURL("https://foo.com")).WithIsPdf(true));
  scoped_refptr<SiteInstanceImpl> pdf_instance =
      SiteInstanceImpl::CreateForUrlInfo(&browser_context, pdf_url_info,
                                         /*is_guest=*/false,
                                         /*is_fenced=*/false,
                                         /*is_fixed_storage_partition=*/false);
  p->LockProcess(pdf_instance->GetIsolationContext(), kRendererID,
                 /*is_process_used=*/false,
                 ProcessLock::FromSiteInfo(pdf_instance->GetSiteInfo()));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  auto bar_origin = url::Origin::Create(GURL("https://bar.com"));

  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  // A PDF process should be able to commit new URLs that match its ProcessLock.
  EXPECT_TRUE(p->CanAccessOrigin(kRendererID, foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(kRendererID, bar_origin,
                                  AccessType::kCanCommitNewOrigin));

  // A PDF process should also be able to claim it's hosting an origin that
  // matches its ProcessLock; for example, PDF documents can still use
  // postMessage so they need to use this to validate the source origin.
  EXPECT_TRUE(
      p->CanAccessOrigin(kRendererID, foo_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(kRendererID, bar_origin, AccessType::kHostsOrigin));

  // A PDF process should not be able to access data for any origin.
  EXPECT_FALSE(p->CanAccessOrigin(
      kRendererID, foo_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      kRendererID, bar_origin, AccessType::kCanAccessDataForCommittedOrigin));

  p->Remove(kRendererID);
}

// Test the granting of origin permissions, and their interactions with
// granting scheme permissions.
TEST_F(ChildProcessSecurityPolicyTest, OriginGranting) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url_foo1(GetWebUIURL("foo/resource1"));
  GURL url_foo2(GetWebUIURL("foo/resource2"));
  GURL url_bar(GetWebUIURL("bar/resource3"));

  p->AddForTesting(kRendererID, browser_context());
  LockProcessIfNeeded(kRendererID, browser_context(), url_foo1);

  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));

  p->GrantRequestOrigin(kRendererID, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));

  p->GrantCommitOrigin(kRendererID, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));

  // Make sure this doesn't overwrite the earlier commit grants.
  p->GrantRequestOrigin(kRendererID, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(kRendererID, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(kRendererID, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(kRendererID, url_bar));

  p->Remove(kRendererID);
}

#define LOCKED_EXPECT_THAT(lock, value, matcher) \
  do {                                           \
    base::AutoLock auto_lock(lock);              \
    EXPECT_THAT(value, matcher);                 \
  } while (0);

// Verifies ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins method.
TEST_F(ChildProcessSecurityPolicyTest, AddFutureIsolatedOrigins) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  url::Origin quxfoo = url::Origin::Create(GURL("https://qux.foo.com/"));
  url::Origin baz_http = url::Origin::Create(GURL("http://baz.com/"));
  url::Origin baz_http_8000 = url::Origin::Create(GURL("http://baz.com:8000/"));
  url::Origin baz_https_8000 =
      url::Origin::Create(GURL("https://baz.com:8000/"));
  url::Origin invalid_etld = url::Origin::Create(GURL("https://gov/"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Verify deduplication of the argument.
  p->AddFutureIsolatedOrigins({foo, bar, bar}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(foo),
                                    GetIsolatedOriginEntry(bar)));

  // Verify that the old set is extended (not replaced).
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(foo),
                                    GetIsolatedOriginEntry(bar),
                                    GetIsolatedOriginEntry(baz)));

  // Verify deduplication against the old set.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(foo),
                                    GetIsolatedOriginEntry(bar),
                                    GetIsolatedOriginEntry(baz)));

  // Verify deduplication considers scheme and port differences.  Note that
  // origins that differ only in ports map to the same key.
  p->AddFutureIsolatedOrigins({baz, baz_http_8000, baz_https_8000},
                              IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(
          GetIsolatedOriginEntry(foo), GetIsolatedOriginEntry(bar),
          GetIsolatedOriginEntry(baz), GetIsolatedOriginEntry(baz_http)));

  // Verify that adding an origin that is invalid for isolation will 1) log a
  // warning and 2) won't CHECK or crash the browser process, 3) will not add
  // the invalid origin, but will add the remaining origins passed to
  // AddFutureIsolatedOrigins.  Note that the new |quxfoo| origin should map to
  // the same key (i.e., the https://foo.com/ site URL) as the existing |foo|
  // origin.
  {
    base::test::MockLog mock_log;
    EXPECT_CALL(mock_log,
                Log(::logging::LOGGING_ERROR, testing::_, testing::_,
                    testing::_, testing::HasSubstr(invalid_etld.Serialize())))
        .Times(1);

    mock_log.StartCapturingLogs();
    p->AddFutureIsolatedOrigins({quxfoo, invalid_etld},
                                IsolatedOriginSource::TEST);
    LOCKED_EXPECT_THAT(
        p->isolated_origins_lock_, p->isolated_origins_,
        testing::UnorderedElementsAre(
            GetIsolatedOriginEntry(foo, quxfoo), GetIsolatedOriginEntry(bar),
            GetIsolatedOriginEntry(baz), GetIsolatedOriginEntry(baz_http)));
  }

  // Verify that adding invalid origins via the string variant of
  // AddFutureIsolatedOrigins() logs a warning.
  {
    base::test::MockLog mock_log;
    EXPECT_CALL(mock_log, Log(::logging::LOGGING_ERROR, testing::_, testing::_,
                              testing::_, testing::HasSubstr("about:blank")))
        .Times(1);

    mock_log.StartCapturingLogs();
    p->AddFutureIsolatedOrigins("about:blank", IsolatedOriginSource::TEST);
  }

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(quxfoo);
  p->RemoveIsolatedOriginForTesting(bar);
  p->RemoveIsolatedOriginForTesting(baz);
  p->RemoveIsolatedOriginForTesting(baz_http);

  // We should have removed all isolated origins at this point.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());
}

TEST_F(ChildProcessSecurityPolicyTest, IsolateAllSuborigins) {
  url::Origin qux = url::Origin::Create(GURL("https://qux.com/"));
  IsolatedOriginPattern etld1_wild("https://[*.]foo.com");
  IsolatedOriginPattern etld2_wild("https://[*.]bar.foo.com");
  url::Origin etld1 = url::Origin::Create(GURL("https://foo.com"));
  url::Origin etld2 = url::Origin::Create(GURL("https://bar.foo.com"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Check we can add a single wildcard origin.
  p->AddFutureIsolatedOrigins({etld1_wild}, IsolatedOriginSource::TEST);

  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(etld1, true)));

  // Add a conventional origin and check they can live side by side.
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(etld1, true),
                                    GetIsolatedOriginEntry(qux, false)));

  // Check that a wildcard domain within another wildcard domain can be added.
  p->AddFutureIsolatedOrigins({etld2_wild}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::UnorderedElementsAre(
                         GetIsolatedOriginEntry(etld1, etld2, true, true),
                         GetIsolatedOriginEntry(qux, false)));

  // Check that removing a single wildcard domain, that contains another
  // wildcard domain, doesn't affect the isolating behavior of the original
  // wildcard domain.
  p->RemoveIsolatedOriginForTesting(etld1);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(etld2, true),
                                    GetIsolatedOriginEntry(qux, false)));

  // Removing remaining domains.
  p->RemoveIsolatedOriginForTesting(qux);
  p->RemoveIsolatedOriginForTesting(etld2);

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());
}

// Verify that the isolation behavior for wildcard and non-wildcard origins,
// singly or in concert, behaves correctly via calls to GetSiteForURL().
TEST_F(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       WildcardAndNonWildcardOrigins) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // There should be no isolated origins before this test starts.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Construct a simple case, a single isolated origin.
  //  IsolatedOriginPattern isolated("https://isolated.com");
  IsolatedOriginPattern inner_isolated("https://inner.isolated.com");
  IsolatedOriginPattern wildcard("https://[*.]wildcard.com");
  IsolatedOriginPattern inner_wildcard("https://[*.]inner.wildcard.com");

  GURL isolated_url("https://isolated.com");
  GURL inner_isolated_url("https://inner.isolated.com");
  GURL host_inner_isolated_url("https://host.inner.isolated.com");
  GURL wildcard_url("https://wildcard.com");
  GURL inner_wildcard_url("https://inner.wildcard.com");
  GURL host_inner_wildcard_url("https://host.inner.wildcard.com");
  GURL unrelated_url("https://unrelated.com");

  // Verify the isolation behavior of the test patterns before isolating any
  // domains.
  std::map<GURL, GURL> origins_site_test_map{
      {isolated_url, isolated_url},
      {inner_isolated_url, isolated_url},
      {host_inner_isolated_url, isolated_url},
      {wildcard_url, wildcard_url},
      {inner_wildcard_url, wildcard_url},
      {host_inner_wildcard_url, wildcard_url},
      {unrelated_url, unrelated_url},
  };
  CheckGetSiteForURL(browser_context(), origins_site_test_map);

  // Add |wildcard|, a wildcard origin from a different domain, then verify that
  // the existing behavior of |isolated_url| and |inner_isolated_url| remains
  // unaffected, while all subdomains of wildcard.com are returned as unique
  // sites.
  p->AddFutureIsolatedOrigins({wildcard}, IsolatedOriginSource::TEST);
  origins_site_test_map[inner_wildcard_url] = inner_wildcard_url;
  origins_site_test_map[host_inner_wildcard_url] = host_inner_wildcard_url;
  CheckGetSiteForURL(browser_context(), origins_site_test_map);

  // Add |inner_isolated|, then verify that querying for |inner_isolated_url|
  // returns |inner_isolated_url| while leaving the wildcard origins unaffected.
  p->AddFutureIsolatedOrigins({inner_isolated}, IsolatedOriginSource::TEST);
  origins_site_test_map[inner_isolated_url] = inner_isolated_url;
  origins_site_test_map[host_inner_isolated_url] = inner_isolated_url;
  CheckGetSiteForURL(browser_context(), origins_site_test_map);

  // Add |inner_wildcard|. This should not change the behavior of the test
  // above as all subdomains of |inner_wildcard| are contained within
  // |wildcard|.
  p->AddFutureIsolatedOrigins({inner_wildcard}, IsolatedOriginSource::TEST);
  CheckGetSiteForURL(browser_context(), origins_site_test_map);

  p->RemoveIsolatedOriginForTesting(wildcard.origin());
  p->RemoveIsolatedOriginForTesting(inner_isolated.origin());
  p->RemoveIsolatedOriginForTesting(inner_wildcard.origin());

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());
}

TEST_F(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       WildcardAndNonWildcardEmbedded) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // There should be no isolated origins before this test starts.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  {
    // Test the behavior of a wildcard origin contained within a single
    // isolated origin. Removing the isolated origin should have no effect on
    // the wildcard origin.
    IsolatedOriginPattern isolated("https://isolated.com");
    IsolatedOriginPattern wildcard_isolated(
        "https://[*.]wildcard.isolated.com");

    GURL isolated_url("https://isolated.com");
    GURL a_isolated_url("https://a.isolated.com");
    GURL wildcard_isolated_url("https://wildcard.isolated.com");
    GURL a_wildcard_isolated_url("https://a.wildcard.isolated.com");

    p->AddFutureIsolatedOrigins({isolated, wildcard_isolated},
                                IsolatedOriginSource::TEST);
    std::map<GURL, GURL> origin_site_map{
        {isolated_url, isolated_url},
        {a_isolated_url, isolated_url},
        {wildcard_isolated_url, wildcard_isolated_url},
        {a_wildcard_isolated_url, a_wildcard_isolated_url},
    };

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->RemoveIsolatedOriginForTesting(isolated.origin());
    p->RemoveIsolatedOriginForTesting(wildcard_isolated.origin());
  }

  // No isolated origins should persist between tests.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  {
    // A single isolated origin is nested within a wildcard origin. In this
    // scenario the wildcard origin supersedes isolated origins.
    IsolatedOriginPattern wildcard("https://[*.]wildcard.com");
    IsolatedOriginPattern isolated_wildcard("https://isolated.wildcard.com");

    GURL wildcard_url("https://wildcard.com");
    GURL a_wildcard_url("https://a.wildcard.com");
    GURL isolated_wildcard_url("https://isolated.wildcard.com");
    GURL a_isolated_wildcard_url("https://a.isolated.wildcard.com");

    p->AddFutureIsolatedOrigins({wildcard, isolated_wildcard},
                                IsolatedOriginSource::TEST);
    std::map<GURL, GURL> origin_site_map{
        {wildcard_url, wildcard_url},
        {a_wildcard_url, a_wildcard_url},
        {isolated_wildcard_url, isolated_wildcard_url},
        {a_isolated_wildcard_url, a_isolated_wildcard_url},
    };

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->RemoveIsolatedOriginForTesting(wildcard.origin());
    p->RemoveIsolatedOriginForTesting(isolated_wildcard.origin());
  }

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  {
    // Nest wildcard isolated origins within each other. Verify that removing
    // the outer wildcard origin doesn't affect the inner one.
    IsolatedOriginPattern outer("https://[*.]outer.com");
    IsolatedOriginPattern inner("https://[*.]inner.outer.com");

    GURL outer_url("https://outer.com");
    GURL a_outer_url("https://a.outer.com");
    GURL inner_url("https://inner.outer.com");
    GURL a_inner_url("https://a.inner.outer.com");

    p->AddFutureIsolatedOrigins({inner, outer}, IsolatedOriginSource::TEST);

    std::map<GURL, GURL> origin_site_map{
        {outer_url, outer_url},
        {a_outer_url, a_outer_url},
        {inner_url, inner_url},
        {a_inner_url, a_inner_url},
    };

    CheckGetSiteForURL(browser_context(), origin_site_map);
    p->RemoveIsolatedOriginForTesting(outer.origin());
    p->RemoveIsolatedOriginForTesting(inner.origin());
  }

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Verify that adding a wildcard domain then a then a conventional domain
  // doesn't affect the isolating behavior of the wildcard, i.e. whichever
  // isolated domain is added entered 'wins'.
  {
    IsolatedOriginPattern wild("https://[*.]bar.foo.com");
    IsolatedOriginPattern single("https://bar.foo.com");

    GURL host_url("https://host.bar.foo.com");

    p->AddFutureIsolatedOrigins({wild}, IsolatedOriginSource::TEST);
    std::map<GURL, GURL> origin_site_map{
        {host_url, host_url},
    };

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->AddFutureIsolatedOrigins({single}, IsolatedOriginSource::TEST);

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->RemoveIsolatedOriginForTesting(wild.origin());
    p->RemoveIsolatedOriginForTesting(single.origin());
  }

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Verify the first domain added remains dominant in the case of differing
  // wildcard and non-wildcard statuses.
  {
    IsolatedOriginPattern wild("https://[*.]bar.foo.com");
    IsolatedOriginPattern single("https://bar.foo.com");

    GURL host_url("https://host.bar.foo.com");
    GURL domain_url("https://bar.foo.com");

    p->AddFutureIsolatedOrigins({single}, IsolatedOriginSource::TEST);
    std::map<GURL, GURL> origin_site_map{
        {host_url, domain_url},
    };

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->AddFutureIsolatedOrigins({wild}, IsolatedOriginSource::TEST);

    CheckGetSiteForURL(browser_context(), origin_site_map);

    p->RemoveIsolatedOriginForTesting(wild.origin());
    p->RemoveIsolatedOriginForTesting(single.origin());
  }

  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());
}

// Verifies that isolated origins only apply to future BrowsingInstances.
TEST_F(ChildProcessSecurityPolicyTest, DynamicIsolatedOrigins) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  url::Origin qux = url::Origin::Create(GURL("https://qux.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Save the next BrowsingInstance ID to be created.  Because unit tests run
  // in batches, this isn't guaranteed to always be 1, for example if a
  // previous test in the same batch had already created a SiteInstance and
  // BrowsingInstance.
  BrowsingInstanceId initial_id(SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate foo.com and bar.com.
  p->AddFutureIsolatedOrigins({foo, bar}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(initial_id, foo),
                                    GetIsolatedOriginEntry(initial_id, bar)));

  // Isolating bar.com again should have no effect.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(initial_id, foo),
                                    GetIsolatedOriginEntry(initial_id, bar)));

  // Create a new BrowsingInstance.  Its ID will be |initial_id|.
  TestBrowserContext context;
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com/"));
  EXPECT_EQ(initial_id,
            foo_instance->GetIsolationContext().browsing_instance_id());
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 1),
            SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate baz.com.  This will apply to BrowsingInstances with IDs
  // |initial_id + 1| and above.
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::UnorderedElementsAre(
                         GetIsolatedOriginEntry(initial_id, foo),
                         GetIsolatedOriginEntry(initial_id, bar),
                         GetIsolatedOriginEntry(initial_id.value() + 1, baz)));

  // Isolating bar.com again should not update the old BrowsingInstance ID.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::UnorderedElementsAre(
                         GetIsolatedOriginEntry(initial_id, foo),
                         GetIsolatedOriginEntry(initial_id, bar),
                         GetIsolatedOriginEntry(initial_id.value() + 1, baz)));

  // Create another BrowsingInstance.
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com/"));
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 1),
            bar_instance->GetIsolationContext().browsing_instance_id());
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 2),
            SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate qux.com.
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::UnorderedElementsAre(
                         GetIsolatedOriginEntry(initial_id, foo),
                         GetIsolatedOriginEntry(initial_id, bar),
                         GetIsolatedOriginEntry(initial_id.value() + 1, baz),
                         GetIsolatedOriginEntry(initial_id.value() + 2, qux)));

  // Check IsIsolatedOrigin() only returns isolated origins if they apply to
  // the provided BrowsingInstance. foo and bar should apply in
  // BrowsingInstance ID |initial_id| and above, baz in IDs |initial_id + 1|
  // and above, and qux in |initial_id + 2| and above.
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, initial_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, initial_id, qux));

  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 1, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 1, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 1, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, initial_id.value() + 1, qux));

  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 2, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 2, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 2, baz));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 2, qux));

  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 42, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 42, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 42, baz));
  EXPECT_TRUE(IsIsolatedOrigin(&context, initial_id.value() + 42, qux));

  // An IsolationContext constructed without a BrowsingInstance ID should
  // return the latest available isolated origins.
  EXPECT_TRUE(p->IsIsolatedOrigin(IsolationContext(&context), foo,
                                  false /* origin_requests_isolation */));
  EXPECT_TRUE(p->IsIsolatedOrigin(IsolationContext(&context), bar,
                                  false /* origin_requests_isolation */));
  EXPECT_TRUE(p->IsIsolatedOrigin(IsolationContext(&context), baz,
                                  false /* origin_requests_isolation */));
  EXPECT_TRUE(p->IsIsolatedOrigin(IsolationContext(&context), qux,
                                  false /* origin_requests_isolation */));

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(bar);
  p->RemoveIsolatedOriginForTesting(baz);
  p->RemoveIsolatedOriginForTesting(qux);
}

// Check that an unsuccessful isolated origin lookup for a URL with an empty
// host doesn't crash. See https://crbug.com/882686.
TEST_F(ChildProcessSecurityPolicyTest, IsIsolatedOriginWithEmptyHost) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context;
  EXPECT_FALSE(p->IsIsolatedOrigin(IsolationContext(&context),
                                   url::Origin::Create(GURL()),
                                   false /* origin_requests_isolation */));
  EXPECT_FALSE(p->IsIsolatedOrigin(IsolationContext(&context),
                                   url::Origin::Create(GURL("file:///foo")),
                                   false /* origin_requests_isolation */));
}

// Verifies the API for restricting isolated origins to a specific
// BrowserContext (profile).  Namely, the same origin may be added for
// different BrowserContexts, possibly with different BrowsingInstanceId
// cutoffs.  Attempts to re-add an origin for the same profile should be
// ignored.  Also, once an isolated origin is added globally for all profiles,
// future attempts to re-add it (for any profile) should also be ignored.
TEST_F(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForSpecificBrowserContexts) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Save the next BrowsingInstance ID to be created.  Because unit tests run
  // in batches, this isn't guaranteed to always be 1, for example if a
  // previous test in the same batch had already created a SiteInstance and
  // BrowsingInstance.
  BrowsingInstanceId initial_id(SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate foo.com globally (for all BrowserContexts).
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST);

  TestBrowserContext context1, context2;

  // Isolate bar.com in |context1|.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context1);

  // bar.com should be isolated for |context1|, but not |context2|. foo.com
  // should be isolated for all contexts.
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context2, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context2, initial_id, bar));

  // Create a new BrowsingInstance.  Its ID will be |initial_id|.
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context1, GURL("https://foo.com/"));
  EXPECT_EQ(initial_id,
            foo_instance->GetIsolationContext().browsing_instance_id());
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 1),
            SiteInstanceImpl::NextBrowsingInstanceId());
  EXPECT_EQ(&context1, foo_instance->GetIsolationContext()
                           .browser_or_resource_context()
                           .ToBrowserContext());

  // Isolating foo.com in |context1| is allowed and should add a new
  // IsolatedOriginEntry.  This wouldn't introduce any additional isolation,
  // since foo.com is already isolated globally, but the new entry is
  // important, e.g. for persisting profile-specific isolated origins across
  // restarts.
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(foo));
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST, &context1);
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context2, initial_id, foo));

  // Isolating bar.com in |context1| again should have no effect.
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(bar));
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context1);
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context2, initial_id, bar));

  // Isolate bar.com for |context2|, which should add a new
  // IsolatedOriginEntry.  Verify that the isolation took effect for
  // |initial_id + 1| (the current BrowsingInstance ID cutoff) only.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context2);
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context2, initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context2, initial_id.value() + 1, bar));

  // Verify the bar.com is still isolated in |context1| starting with
  // |initial_id|.
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id.value() + 1, bar));

  // Create another BrowserContext; only foo.com should be isolated there.
  TestBrowserContext context3;
  EXPECT_TRUE(IsIsolatedOrigin(&context3, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context3, initial_id.value() + 1, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context3, initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context3, initial_id.value() + 1, bar));

  // Now, add bar.com as a globally isolated origin.  This should make it apply
  // to context3 as well, but only in initial_id + 1 (the current
  // BrowsingInstance ID cutoff).
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST);
  EXPECT_EQ(3, GetIsolatedOriginEntryCount(bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context3, initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context3, initial_id.value() + 1, bar));

  // An attempt to re-add bar.com for a new profile should create a new
  // IsolatedOriginEntry, though it wouldn't provide any additional isolation,
  // since bar.com is already isolated globally.
  TestBrowserContext context4;
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context4);
  EXPECT_EQ(4, GetIsolatedOriginEntryCount(bar));

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(bar);
}

// This test ensures that isolated origins associated with a specific
// BrowserContext are removed when that BrowserContext is destroyed.
TEST_F(ChildProcessSecurityPolicyTest,
       IsolatedOriginsRemovedWhenBrowserContextDestroyed) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin sub_foo = url::Origin::Create(GURL("https://sub.foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Save the next BrowsingInstance ID to be created.  Because unit tests run
  // in batches, this isn't guaranteed to always be 1, for example if a
  // previous test in the same batch had already created a SiteInstance and
  // BrowsingInstance.
  BrowsingInstanceId initial_id(SiteInstanceImpl::NextBrowsingInstanceId());

  std::unique_ptr<TestBrowserContext> context1(new TestBrowserContext());
  std::unique_ptr<TestBrowserContext> context2(new TestBrowserContext());

  // Isolate foo.com in |context1|.  Note that sub.foo.com should also be
  // considered isolated in |context1|, since it's a subdomain of foo.com.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST,
                              context1.get());
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(foo));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, sub_foo));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));

  // Isolate sub.foo.com and bar.com in |context2|.
  p->AddFutureIsolatedOrigins({sub_foo, bar}, IsolatedOriginSource::TEST,
                              context2.get());
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(sub_foo));
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(bar));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, foo));

  // Isolate baz.com in both BrowserContexts.
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST,
                              context1.get());
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST,
                              context2.get());

  EXPECT_EQ(2, GetIsolatedOriginEntryCount(baz));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, baz));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, baz));

  // Remove |context1|.  foo.com should no longer be in the isolated_origins_
  // map, and the other origins should be isolated only in |context2|.
  context1.reset();

  EXPECT_EQ(0, GetIsolatedOriginEntryCount(foo));
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(sub_foo));
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(bar));
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(baz));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, baz));

  // Remove |context2| and ensure the remaining entries are removed.
  context2.reset();
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());
}

TEST_F(ChildProcessSecurityPolicyTest, IsolatedOriginPattern) {
  const std::string_view etld1_wild("https://[*.]foo.com");
  url::Origin etld1_wild_origin = url::Origin::Create(GURL("https://foo.com"));
  IsolatedOriginPattern p(etld1_wild);
  EXPECT_TRUE(p.isolate_all_subdomains());
  EXPECT_TRUE(p.is_valid());
  EXPECT_EQ(p.origin(), etld1_wild_origin);

  const std::string_view etld2_wild("https://[*.]bar.foo.com");
  url::Origin etld2_wild_origin =
      url::Origin::Create(GURL("https://bar.foo.com"));
  bool result = p.Parse(etld2_wild);
  EXPECT_TRUE(result);
  EXPECT_TRUE(p.isolate_all_subdomains());
  EXPECT_TRUE(p.is_valid());
  EXPECT_EQ(p.origin(), etld2_wild_origin);
  EXPECT_FALSE(p.origin().opaque());

  const std::string_view etld1("https://baz.com");
  url::Origin etld1_origin = url::Origin::Create(GURL("https://baz.com"));
  result = p.Parse(etld1);
  EXPECT_TRUE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_TRUE(p.is_valid());
  EXPECT_EQ(p.origin(), etld1_origin);
  EXPECT_FALSE(p.origin().opaque());

  const std::string_view bad_scheme("ftp://foo.com");
  result = p.Parse(bad_scheme);
  EXPECT_FALSE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.is_valid());
  EXPECT_TRUE(p.origin().opaque());

  const std::string_view no_scheme_sep("httpsfoo.com");
  result = p.Parse(no_scheme_sep);
  EXPECT_FALSE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.is_valid());
  EXPECT_TRUE(p.origin().opaque());

  const std::string_view bad_registry("https://co.uk");
  result = p.Parse(bad_registry);
  EXPECT_FALSE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.is_valid());
  EXPECT_TRUE(p.origin().opaque());

  const std::string_view trailing_dot("https://bar.com.");
  result = p.Parse(trailing_dot);
  EXPECT_FALSE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.is_valid());
  EXPECT_TRUE(p.origin().opaque());

  const std::string_view ip_addr("https://10.20.30.40");
  url::Origin ip_origin = url::Origin::Create(GURL("https://10.20.30.40"));
  result = p.Parse(ip_addr);
  EXPECT_TRUE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.origin().opaque());
  EXPECT_TRUE(p.is_valid());
  EXPECT_EQ(p.origin(), ip_origin);

  const std::string_view wild_ip_addr("https://[*.]10.20.30.40");
  result = p.Parse(wild_ip_addr);
  EXPECT_FALSE(result);
  EXPECT_FALSE(p.isolate_all_subdomains());
  EXPECT_FALSE(p.is_valid());

  const url::Origin bad_origin;
  IsolatedOriginPattern bad_pattern(bad_origin);
  EXPECT_FALSE(bad_pattern.isolate_all_subdomains());
  EXPECT_TRUE(bad_pattern.origin().opaque());
  EXPECT_FALSE(p.is_valid());
}

// This test adds isolated origins from various sources and verifies that
// GetIsolatedOrigins() properly restricts lookups by source.
TEST_F(ChildProcessSecurityPolicyTest, GetIsolatedOrigins) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  url::Origin qux = url::Origin::Create(GURL("https://qux.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Add isolated origins from various sources, and verify that
  // GetIsolatedOrigins properly restricts lookups by source.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST);
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::FIELD_TRIAL);

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(foo, bar));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::TEST),
              testing::UnorderedElementsAre(foo));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::FIELD_TRIAL),
              testing::UnorderedElementsAre(bar));

  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::POLICY);
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::COMMAND_LINE);

  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz, qux));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::TEST),
              testing::UnorderedElementsAre(foo));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::FIELD_TRIAL),
              testing::UnorderedElementsAre(bar));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::POLICY),
              testing::UnorderedElementsAre(baz));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::COMMAND_LINE),
              testing::UnorderedElementsAre(qux));

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(bar);
  p->RemoveIsolatedOriginForTesting(baz);
  p->RemoveIsolatedOriginForTesting(qux);
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

// This test adds isolated origins from various sources as well as restricted
// to particular profiles, and verifies that GetIsolatedOrigins() properly
// restricts lookups by both source and profile.
TEST_F(ChildProcessSecurityPolicyTest, GetIsolatedOriginsWithProfile) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  url::Origin qux = url::Origin::Create(GURL("https://qux.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context1, context2;

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Add a global isolated origin.  Note that since it applies to all profiles,
  // GetIsolatedOrigins() should return it for any passed-in profile.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST);

  // Add some per-profile isolated origins.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::USER_TRIGGERED,
                              &context1);
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::POLICY, &context2);
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::USER_TRIGGERED,
                              &context1);
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::USER_TRIGGERED,
                              &context2);

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(foo));

  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::TEST),
              testing::UnorderedElementsAre(foo));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::TEST, &context1),
              testing::UnorderedElementsAre(foo));
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::TEST, &context2),
              testing::UnorderedElementsAre(foo));

  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::USER_TRIGGERED),
              testing::IsEmpty());
  EXPECT_THAT(
      p->GetIsolatedOrigins(IsolatedOriginSource::USER_TRIGGERED, &context1),
      testing::UnorderedElementsAre(bar, qux));
  EXPECT_THAT(
      p->GetIsolatedOrigins(IsolatedOriginSource::USER_TRIGGERED, &context2),
      testing::UnorderedElementsAre(qux));

  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::POLICY),
              testing::IsEmpty());
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::POLICY, &context1),
              testing::IsEmpty());
  EXPECT_THAT(p->GetIsolatedOrigins(IsolatedOriginSource::POLICY, &context2),
              testing::UnorderedElementsAre(baz));

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(bar);
  p->RemoveIsolatedOriginForTesting(baz);
  p->RemoveIsolatedOriginForTesting(qux);
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

TEST_F(ChildProcessSecurityPolicyTest, IsolatedOriginPatternEquality) {
  std::string foo("https://foo.com");
  std::string foo_port("https://foo.com:8000");
  std::string foo_path("https://foo.com/some/path");

  EXPECT_EQ(IsolatedOriginPattern(foo), IsolatedOriginPattern(foo_port));
  EXPECT_EQ(IsolatedOriginPattern(foo), IsolatedOriginPattern(foo_path));

  std::string wild_foo("https://[*.]foo.com");
  std::string wild_foo_port("https://[*.]foo.com:8000");
  std::string wild_foo_path("https://[*.]foo.com/some/path");

  EXPECT_EQ(IsolatedOriginPattern(wild_foo),
            IsolatedOriginPattern(wild_foo_port));
  EXPECT_EQ(IsolatedOriginPattern(wild_foo),
            IsolatedOriginPattern(wild_foo_path));

  EXPECT_FALSE(IsolatedOriginPattern(foo) == IsolatedOriginPattern(wild_foo));
}

// Verifies parsing logic in SiteIsolationPolicy::ParseIsolatedOrigins.
TEST_F(ChildProcessSecurityPolicyTest, ParseIsolatedOrigins) {
  EXPECT_THAT(ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(""),
              testing::IsEmpty());

  // Single simple, valid origin.
  EXPECT_THAT(
      ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
          "http://isolated.foo.com"),
      testing::ElementsAre(IsolatedOriginPattern("http://isolated.foo.com")));

  // Multiple comma-separated origins.
  EXPECT_THAT(
      ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
          "http://a.com,https://b.com,,https://c.com:8000"),
      testing::ElementsAre(IsolatedOriginPattern("http://a.com"),
                           IsolatedOriginPattern("https://b.com"),
                           IsolatedOriginPattern("https://c.com:8000")));

  // ParseIsolatedOrigins should not do any deduplication (that is the job of
  // ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins).
  EXPECT_THAT(
      ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
          "https://b.com,https://b.com,https://b.com:1234"),
      testing::ElementsAre(IsolatedOriginPattern("https://b.com"),
                           IsolatedOriginPattern("https://b.com"),
                           IsolatedOriginPattern("https://b.com:1234")));

  // A single wildcard origin.
  EXPECT_THAT(
      ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
          "https://[*.]wild.foo.com"),
      testing::ElementsAre(IsolatedOriginPattern("https://[*.]wild.foo.com")));

  // A mixture of wildcard and non-wildcard origins.
  EXPECT_THAT(
      ChildProcessSecurityPolicyImpl::ParseIsolatedOrigins(
          "https://[*.]wild.foo.com,https://isolated.foo.com"),
      testing::ElementsAre(IsolatedOriginPattern("https://[*.]wild.foo.com"),
                           IsolatedOriginPattern("https://isolated.foo.com")));
}

// Verify that the default port for an isolated origin's scheme is returned
// during a lookup, not the port of the origin requested.
TEST_F(ChildProcessSecurityPolicyTest, WildcardDefaultPort) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  url::Origin isolated_origin_with_port =
      url::Origin::Create(GURL("https://isolated.com:1234"));
  url::Origin isolated_origin =
      url::Origin::Create(GURL("https://isolated.com"));

  url::Origin wild_with_port =
      url::Origin::Create(GURL("https://a.wild.com:5678"));
  url::Origin wild_origin = url::Origin::Create(GURL("https://a.wild.com"));
  IsolatedOriginPattern wild_pattern("https://[*.]wild.com:5678");

  p->AddFutureIsolatedOrigins({isolated_origin_with_port},
                              IsolatedOriginSource::TEST);
  p->AddFutureIsolatedOrigins({wild_pattern}, IsolatedOriginSource::TEST);

  IsolationContext isolation_context(browser_context());
  url::Origin lookup_origin;

  // Requesting isolated_origin_with_port should return the same origin but with
  // the default port for the scheme.
  const bool kOriginRequestsIsolation = false;
  EXPECT_TRUE(p->GetMatchingProcessIsolatedOrigin(
      isolation_context, isolated_origin_with_port, kOriginRequestsIsolation,
      &lookup_origin));
  EXPECT_EQ(url::DefaultPortForScheme(lookup_origin.scheme()),
            lookup_origin.port());
  EXPECT_EQ(isolated_origin, lookup_origin);

  p->RemoveIsolatedOriginForTesting(isolated_origin);

  // Similarly, looking up matching isolated origins for wildcard origins must
  // also return the default port for the origin's scheme, not the report of the
  // requested origin.
  EXPECT_TRUE(p->GetMatchingProcessIsolatedOrigin(
      isolation_context, wild_with_port, kOriginRequestsIsolation,
      &lookup_origin));
  EXPECT_EQ(url::DefaultPortForScheme(lookup_origin.scheme()),
            lookup_origin.port());
  EXPECT_EQ(wild_origin, lookup_origin);

  p->RemoveIsolatedOriginForTesting(wild_pattern.origin());

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

TEST_F(ChildProcessSecurityPolicyTest, ProcessLockMatching) {
  GURL nonapp_url("https://bar.com/");
  GURL app_url("https://some.app.foo.com/");
  GURL app_effective_url("https://app.com/");
  EffectiveURLContentBrowserClient modified_client(
      app_url, app_effective_url, /* requires_dedicated_process */ true);
  ContentBrowserClient* original_client =
      SetBrowserClientForTesting(&modified_client);

  IsolationContext isolation_context(browser_context());

  auto nonapp_urlinfo = UrlInfo::CreateForTesting(
      nonapp_url, CreateStoragePartitionConfigForTesting());
  auto ui_nonapp_url_siteinfo =
      SiteInfo::Create(isolation_context, nonapp_urlinfo);
  auto ui_nonapp_url_lock =
      ProcessLock::Create(isolation_context, nonapp_urlinfo);

  auto app_urlinfo = UrlInfo::CreateForTesting(
      app_url, CreateStoragePartitionConfigForTesting());
  auto ui_app_url_lock = ProcessLock::Create(isolation_context, app_urlinfo);
  auto ui_app_url_siteinfo = SiteInfo::Create(isolation_context, app_urlinfo);

  SiteInfo io_nonapp_url_siteinfo(browser_context());
  ProcessLock io_nonapp_url_lock;
  SiteInfo io_app_url_siteinfo(browser_context());
  ProcessLock io_app_url_lock;

  base::WaitableEvent io_locks_set_event;

  // Post a task that will compute ProcessLocks for the same URLs in the
  // IO thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        io_nonapp_url_siteinfo =
            SiteInfo::CreateOnIOThread(isolation_context, nonapp_urlinfo);
        io_nonapp_url_lock =
            ProcessLock::Create(isolation_context, nonapp_urlinfo);

        io_app_url_siteinfo =
            SiteInfo::CreateOnIOThread(isolation_context, app_urlinfo);
        io_app_url_lock = ProcessLock::Create(isolation_context, app_urlinfo);

        // Tell the UI thread have computed the locks.
        io_locks_set_event.Signal();
      }));

  io_locks_set_event.Wait();

  // Expect URLs with effective URLs that match the original URL to have
  // matching SiteInfos and matching ProcessLocks.
  EXPECT_EQ(ui_nonapp_url_siteinfo, io_nonapp_url_siteinfo);
  EXPECT_EQ(ui_nonapp_url_lock, io_nonapp_url_lock);

  // Expect hosted app URLs where the effective URL does not match the original
  // URL to have different SiteInfos but matching process locks. The SiteInfos,
  // are expected to be different because the effective URL cannot be computed
  // from the IO thread. This means the site_url fields will differ.
  EXPECT_NE(ui_app_url_siteinfo, io_app_url_siteinfo);
  EXPECT_NE(ui_app_url_siteinfo.site_url(), io_app_url_siteinfo.site_url());
  EXPECT_EQ(ui_app_url_siteinfo.process_lock_url(),
            io_app_url_siteinfo.process_lock_url());
  EXPECT_EQ(ui_app_url_lock, io_app_url_lock);

  SetBrowserClientForTesting(original_client);
}

// Verify the mechanism that allows non-origin-keyed isolated origins to be
// associated with a single BrowsingInstance.
TEST_F(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForSpecificBrowsingInstances) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Create SiteInstances for foo.com, bar.com, and baz.com, with each
  // SiteInstance in a new BrowsingInstance.
  TestBrowserContext context;
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com/"));
  auto foo_browsing_instance_id =
      foo_instance->GetIsolationContext().browsing_instance_id();
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com/"));
  auto bar_browsing_instance_id =
      bar_instance->GetIsolationContext().browsing_instance_id();
  scoped_refptr<SiteInstanceImpl> baz_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://baz.com/"));
  auto baz_browsing_instance_id =
      baz_instance->GetIsolationContext().browsing_instance_id();

  // Isolate foo.com for `foo_instance`'s BrowsingInstance only.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      foo_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(
          &context, false /* applies_to_future_browsing_instances */,
          foo_browsing_instance_id, foo)));

  // Verify that foo.com is isolated only in the `foo_instance`'s
  // BrowsingInstance, and no other origins are isolated in any other
  // BrowsingInstances.
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, baz));

  // Verify that subdomains of foo.com are part of the foo.com
  // isolated origin (i.e., that foo.com is not origin-keyed).
  EXPECT_TRUE(
      IsIsolatedOrigin(&context, foo_browsing_instance_id,
                       url::Origin::Create(GURL("https://sub.foo.com"))));
  EXPECT_TRUE(
      IsIsolatedOrigin(&context, foo_browsing_instance_id,
                       url::Origin::Create(GURL("https://sub2.sub.foo.com"))));

  // Isolating foo.com again in the same BrowsingInstance should have no
  // effect.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      foo_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(foo));
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(GetIsolatedOriginEntry(
          &context, false /* applies_to_future_browsing_instances */,
          foo_browsing_instance_id, foo)));

  // Isolate baz.com in `baz_browsing_instance`'s BrowsingInstance.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      baz_instance->GetIsolationContext(), baz, IsolatedOriginSource::TEST);
  LOCKED_EXPECT_THAT(
      p->isolated_origins_lock_, p->isolated_origins_,
      testing::UnorderedElementsAre(
          GetIsolatedOriginEntry(
              &context, false /* applies_to_future_browsing_instances */,
              foo_browsing_instance_id, foo),
          GetIsolatedOriginEntry(
              &context, false /* applies_to_future_browsing_instances */,
              baz_browsing_instance_id, baz)));

  // Verify that foo.com is isolated in the `foo_instance`'s BrowsingInstance,
  // and baz.com is isolated in `baz_instance`'s BrowsingInstance.
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, baz));

  // Isolate bar.com in foo.com (not bar.com)'s BrowsingInstance.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      foo_instance->GetIsolationContext(), bar, IsolatedOriginSource::TEST);

  // Verify that foo.com and bar.com are both isolated in `foo_instance`'s
  // BrowsingInstance, nothing is isolated in bar_instance's BrowsingInstance,
  // and baz.com is isolated in `baz_instance`'s BrowsingInstance.
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, baz));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, baz));

  // Isolate foo.com in `bar_instance` and `baz_instance`'s BrowsingInstances
  // and verify that this takes effect.  This should result in having three
  // entries for foo.com, one for each BrowsingInstance.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      bar_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  p->AddCoopIsolatedOriginForBrowsingInstance(
      baz_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, baz));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, baz));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, baz));
  EXPECT_EQ(3, GetIsolatedOriginEntryCount(foo));

  // Simulate foo_instance and its BrowsingInstance going away.  This should
  // remove the corresponding BrowsingInstance-specific entries in
  // ChildProcessSecurityPolicy, since they are no longer needed.
  p->SetBrowsingInstanceCleanupDelayForTesting(0);
  foo_instance.reset();
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, baz));

  // Other BrowsingInstances shouldn't be affected.
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, baz));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, baz_browsing_instance_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context, baz_browsing_instance_id, baz));

  p->ClearIsolatedOriginsForTesting();
}

// Verify isolated origins associated with a single BrowsingInstance can be
// combined with isolated origins that apply to future BrowsingInstances.
TEST_F(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForCurrentAndFutureBrowsingInstances) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  LOCKED_EXPECT_THAT(p->isolated_origins_lock_, p->isolated_origins_,
                     testing::IsEmpty());

  // Create a SiteInstance for foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com/"));
  auto foo_browsing_instance_id =
      foo_instance->GetIsolationContext().browsing_instance_id();

  // Isolate foo.com for `foo_instance`'s BrowsingInstance only.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      foo_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(1, GetIsolatedOriginEntryCount(foo));

  // Create a SiteInstance for bar.com in a new BrowsingInstance.
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com/"));
  auto bar_browsing_instance_id =
      bar_instance->GetIsolationContext().browsing_instance_id();

  // Isolate foo.com for all future BrowsingInstances (with IDs `future_id` or
  // above). Note that this shouldn't apply to the existing BrowsingInstances
  // for foo_instance and bar_instance.
  BrowsingInstanceId future_id(SiteInstanceImpl::NextBrowsingInstanceId());
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST, &context);

  // We should now have two entries for foo.com, one for
  // foo_browsing_instance_id, and one for future_id.
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));

  // Verify that foo.com is isolated in the `foo_instance`'s BrowsingInstance,
  // as well as future BrowsingInstance IDs.
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  // Other origins shouldn't be isolated.
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, bar_browsing_instance_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context, future_id, bar));

  // An attempt to add foo.com for a specific BrowsingInstance which has ID
  // greater than `future_id` should be ignored, since that's already covered
  // by the second foo.com entry that applies to future BrowsingInstances.
  scoped_refptr<SiteInstanceImpl> future_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com/"));
  EXPECT_EQ(future_id,
            future_instance->GetIsolationContext().browsing_instance_id());
  p->AddCoopIsolatedOriginForBrowsingInstance(
      future_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));

  // Likewise, an attempt to re-add foo.com for future BrowsingInstances should
  // be ignored.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST, &context);
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));

  // However, we can still add foo.com isolation to a BrowsingInstance that
  // precedes `future_id` and doesn't match `foo_browsing_instance_id`.  Check
  // this with `bar_instance`'s BrowsingInstance.
  EXPECT_LT(bar_browsing_instance_id, future_id);
  p->AddCoopIsolatedOriginForBrowsingInstance(
      bar_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(3, GetIsolatedOriginEntryCount(foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  // When foo_instance and its BrowsingInstance goes away, the corresponding
  // entry just for that BrowsingInstance entry should be destroyed, but other
  // entries should remain.
  p->SetBrowsingInstanceCleanupDelayForTesting(0);
  foo_instance.reset();
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  // Destroying a BrowsingInstance with ID `future_id` shouldn't affect the
  // entry that applies to future BrowsingInstances.
  future_instance.reset();
  EXPECT_EQ(2, GetIsolatedOriginEntryCount(foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  p->ClearIsolatedOriginsForTesting();
}

// This test verifies that CanAccessDataForOrigin returns true for a process id
// even if all BrowsingInstanceIDs for that process have been deleted, so long
// as the request matches the process' lock. This test sets an origin-keyed
// lock.
TEST_F(ChildProcessSecurityPolicyTest, NoBrowsingInstanceIDs_OriginKeyed) {
  url::Origin foo = url::Origin::Create(GURL("https://sub.foo.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

  // Create a SiteInstance for sub.foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  {
    auto origin_isolation_request = static_cast<
        UrlInfo::OriginIsolationRequest>(
        UrlInfo::OriginIsolationRequest::kOriginAgentClusterByHeader |
        UrlInfo::OriginIsolationRequest::kRequiresOriginKeyedProcessByHeader);
    UrlInfo url_info(UrlInfoInit(foo.GetURL())
                         .WithOriginIsolationRequest(origin_isolation_request));
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForUrlInfo(
            &context, url_info,
            /*is_guest=*/false,
            /*is_fenced=*/false,
            /*is_fixed_storage_partition=*/false);

    p->Add(kRendererID, &context);
    p->LockProcess(foo_instance->GetIsolationContext(), kRendererID,
                   /*is_process_used=*/false,
                   ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo()));

    EXPECT_TRUE(p->GetProcessLock(kRendererID).is_locked_to_site());
    EXPECT_TRUE(p->GetProcessLock(kRendererID).is_origin_keyed_process());
    EXPECT_EQ(foo.GetURL(), p->GetProcessLock(kRendererID).lock_url());

    EXPECT_TRUE(ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo())
                    .is_origin_keyed_process());
    EXPECT_TRUE(p->DetermineOriginAgentClusterIsolation(
                     foo_instance->GetIsolationContext(), foo,
                     OriginAgentClusterIsolationState::CreateNonIsolated())
                    .requires_origin_keyed_process());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with kRendererID have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(kRendererID));

  // Because the ProcessLock is origin-keyed, we expect sub.foo.com to match but
  // not foo.com.
  EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID, foo));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      kRendererID, url::Origin::Create(GURL("https://foo.com/"))));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      kRendererID, url::Origin::Create(GURL("https://bar.com/"))));

  // We need to remove it otherwise other tests may fail.
  p->Remove(kRendererID);
}

// This test verifies that CanAccessDataForOrigin returns true for a process id
// even if all BrowsingInstanceIDs for that process have been deleted, so long
// as the request matches the process' lock. This test sets a site-keyed lock.
TEST_F(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       NoBrowsingInstanceIDs_SiteKeyed) {
  url::Origin foo = url::Origin::Create(GURL("https://sub.foo.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

  // Create a SiteInstance for sub.foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  {
    p->Add(kRendererID, &context);
    // Isolate foo.com so we can't get a default SiteInstance. This will mean
    // that https://sub.foo.com will end up in a site-keyed SiteInstance, which
    // is what we need.
    p->AddFutureIsolatedOrigins({url::Origin::Create(GURL("https://foo.com"))},
                                IsolatedOriginSource::TEST, &context);

    UrlInfo url_info(UrlInfoInit(foo.GetURL()));
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForUrlInfo(
            &context, url_info,
            /*is_guest=*/false,
            /*is_fenced=*/false,
            /*is_fixed_storage_partition=*/false);
    p->LockProcess(foo_instance->GetIsolationContext(), kRendererID,
                   /*is_process_used=*/false,
                   ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo()));

    EXPECT_TRUE(p->GetProcessLock(kRendererID).is_locked_to_site());
    EXPECT_FALSE(p->GetProcessLock(kRendererID).is_origin_keyed_process());
    EXPECT_EQ(SiteInfo::GetSiteForOrigin(foo),
              p->GetProcessLock(kRendererID).lock_url());

    EXPECT_FALSE(ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo())
                     .is_origin_keyed_process());
    EXPECT_FALSE(p->DetermineOriginAgentClusterIsolation(
                      foo_instance->GetIsolationContext(), foo,
                      OriginAgentClusterIsolationState::CreateNonIsolated())
                     .requires_origin_keyed_process());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with kRendererID have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(kRendererID));

  // Because the ProcessLock is site-keyed, it should match foo.com and all
  // sub-origins.
  EXPECT_TRUE(p->CanAccessDataForOrigin(kRendererID, foo));
  EXPECT_TRUE(p->CanAccessDataForOrigin(
      kRendererID, url::Origin::Create(GURL("https://foo.com/"))));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      kRendererID, url::Origin::Create(GURL("https://bar.com/"))));

  // We need to remove it otherwise other tests may fail.
  p->Remove(kRendererID);
}

// This test verifies that CanAccessDataForOrigin returns false for a process id
// when all BrowsingInstanceIDs for that process have been deleted, and the
// ProcessLock has is_locked_to_site() = false, regardless of the url requested.
TEST_F(ChildProcessSecurityPolicyTest, NoBrowsingInstanceIDs_UnlockedProcess) {
  GURL foo_url = GURL("https://foo.com/");
  url::Origin foo = url::Origin::Create(foo_url);

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

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

  EXPECT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(kRendererID));

  TestBrowserContext context;
  {
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForTesting(&context, foo_url);
    // Adds the process with an "allow_any_site" lock.
    // The next two statements are basically AddForTesting(...), but with a
    // BrowsingInstanceId based on `foo_instance` and not pinned to '1'.
    // This is important when this test is run with other tests, as then
    // BrowsingInstanceId will not be '1' in general.
    p->Add(kRendererID, &context);
    p->LockProcess(foo_instance->GetIsolationContext(), kRendererID,
                   /*is_process_used=*/false,
                   ProcessLock::CreateAllowAnySite(
                       StoragePartitionConfig::CreateDefault(&context),
                       WebExposedIsolationInfo::CreateNonIsolated()));

    EXPECT_TRUE(foo_instance->IsDefaultSiteInstance());
    EXPECT_TRUE(foo_instance->HasSite());
    EXPECT_EQ(foo_instance->GetSiteInfo(),
              SiteInfo::CreateForDefaultSiteInstance(
                  foo_instance->GetIsolationContext(),
                  StoragePartitionConfig::CreateDefault(&context),
                  WebExposedIsolationInfo::CreateNonIsolated()));
    EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with kRendererID have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(kRendererID));

  EXPECT_FALSE(p->GetProcessLock(kRendererID).is_locked_to_site());
  // Ensure that we don't allow the process to keep accessing data for foo after
  // all of the BrowsingInstances are gone, since that would require checking
  // whether foo itself requires a dedicated process.
  EXPECT_FALSE(p->CanAccessDataForOrigin(kRendererID, foo));

  // We need to remove it otherwise other tests may fail.
  p->Remove(kRendererID);
}

// Regression test for https://crbug.com/1324407.
TEST_F(ChildProcessSecurityPolicyTest, CannotLockUsedProcessToSite) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context;

  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com"));
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com"));

  // Start by putting foo.com into an allows-any-site process.
  p->Add(kRendererID, &context);
  p->LockProcess(foo_instance->GetIsolationContext(), kRendererID,
                 /*is_process_used=*/false,
                 ProcessLock::CreateAllowAnySite(
                     StoragePartitionConfig::CreateDefault(&context),
                     WebExposedIsolationInfo::CreateNonIsolated()));
  EXPECT_TRUE(p->GetProcessLock(kRendererID).allows_any_site());
  EXPECT_FALSE(p->GetProcessLock(kRendererID).is_locked_to_site());

  // If the process is then considered used (e.g., by loading content), it
  // should not be possible to lock it to another site.
  EXPECT_CHECK_DEATH_WITH(
      {
        p->LockProcess(bar_instance->GetIsolationContext(), kRendererID,
                       /*is_process_used=*/true,
                       ProcessLock::FromSiteInfo(bar_instance->GetSiteInfo()));
      },
      "Cannot lock an already used process to .*bar\\.com");

  // We need to remove it otherwise other tests may fail.
  p->Remove(kRendererID);
}

}  // namespace content
