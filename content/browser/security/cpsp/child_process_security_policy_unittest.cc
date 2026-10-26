// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/isolated_origin_util.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/site_shard.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_utils.h"
#include "content/test/storage_partition_test_helpers.h"
#include "net/base/filename_util.h"
#include "net/base/schemeful_site.h"
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
    return schemes_.contains(url.GetScheme());
  }

  void ClearSchemes() { schemes_.clear(); }

  void AddScheme(const std::string& scheme) { schemes_.insert(scheme); }

 private:
  std::set<std::string> schemes_;
};

// Locks `process_id` the way SiteInstanceImpl::LockProcessIfNeeded() does when
// the process is assigned to a SiteInstance for `url`. If `url`'s site requires
// a dedicated process, locks the process to that site. Otherwise, or if `url`
// is omitted, locks the process to allow any site, unless it already has such a
// lock.
void LockProcessIfNeeded(ChildProcessId process_id,
                         BrowserContext* browser_context,
                         const std::optional<GURL>& url = std::nullopt) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  scoped_refptr<SiteInstanceImpl> site_instance =
      url ? SiteInstanceImpl::CreateForTesting(browser_context, *url)
          : SiteInstanceImpl::Create(browser_context);
  const IsolationContext& isolation_context =
      site_instance->GetIsolationContext();
  if (url && site_instance->RequiresDedicatedProcess() &&
      site_instance->GetSiteInfo().ShouldLockProcessToSite(isolation_context)) {
    policy->LockProcess(
        isolation_context, process_id,
        ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()));
  } else if (policy->GetProcessLock(process_id).is_invalid()) {
    policy->LockProcess(
        isolation_context, process_id,
        ProcessLock::CreateAllowAnySite(
            StoragePartitionConfig::CreateDefault(browser_context),
            site_instance->GetWebExposedIsolationInfo(),
            /*cross_origin_isolation_key=*/std::nullopt,
            browser_context->UniqueToken()));
  }
}

}  // namespace

// Test parameter used to validate ChildProcessSecurityPolicy behavior across
// all meaningful permutations of the Rust integration feature flags:
// 1. `RustPolicy` controls whether the main CPSP logic runs in `kCppOnly`,
//    `kRustOnly`, or `kRustAndCpp` mode via
//    `features::kChildProcessSecurityPolicyRust`.
// 2. `CpspRustFeature` controls how much of the Rust CPSP feature is enabled
//    (e.g., only the main logic, or also the per-child `ProcessState`
//    management).
using CpspTestParam = std::tuple<RustPolicy, CpspRustFeature>;

class ChildProcessSecurityPolicyTest
    : public testing::Test,
      public ::testing::WithParamInterface<CpspTestParam> {
 public:
  ChildProcessSecurityPolicyTest()
      : task_environment_(BrowserTaskEnvironment::REAL_IO_THREAD),
        old_browser_client_(nullptr) {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    const RustPolicy rust_policy = std::get<0>(GetParam());
    const CpspRustFeature rust_feature = std::get<1>(GetParam());

    // Apply test params to run in three modes: kCppOnly, kRustOnly, and
    // kRustAndCpp. kCppOnly should turn off kChildProcessSecurityPolicyRust,
    // while the other two modes should enable it with a proper FeatureParam to
    // set the mode.
    if (rust_policy == RustPolicy::kCppOnly) {
      disabled_features.push_back(features::kChildProcessSecurityPolicyRust);
    } else {
      enabled_features.push_back(
          {features::kChildProcessSecurityPolicyRust,
           {{kRustPolicyParam.name, kRustPolicyParam.GetName(rust_policy)}}});
    }

    // Additionally enable the ProcessState migration if that CpspRustFeature
    // is enabled.
    if (rust_feature == CpspRustFeature::kProcessState) {
      enabled_features.push_back(
          {features::kChildProcessSecurityPolicyRustProcessState, {}});
    } else {
      disabled_features.push_back(
          features::kChildProcessSecurityPolicyRustProcessState);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    const RustPolicy rust_policy = std::get<0>(info.param);
    const CpspRustFeature rust_feature = std::get<1>(info.param);

    std::string result;
    switch (rust_policy) {
      case RustPolicy::kCppOnly:
        result = "CppOnly";
        break;
      case RustPolicy::kRustOnly:
        result = "RustOnly";
        break;
      case RustPolicy::kRustAndCpp:
        result = "RustAndCpp";
        break;
    }
    result += (rust_feature == CpspRustFeature::kProcessState
                   ? "_ProcessStateEnabled"
                   : "_ProcessStateDisabled");
    return result;
  }

  bool IsRustProcessStateEnabled() const {
    return std::get<1>(GetParam()) == CpspRustFeature::kProcessState;
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
#if BUILDFLAG(IS_CHROMEOS)
    test_browser_client_.AddScheme(content::kExternalFileScheme);
#endif
    SiteIsolationPolicy::DisableFlagCachingForTesting();

    // With unit tests, it's possible that the same global
    // ChildProcessSecurityPolicyImpl instance is reused while running multiple
    // tests, possibly in different Rust modes. So, it's not sufficient to rely
    // on the ChildProcessSecurityPolicyImpl constructor to register web-safe
    // and pseudo schemes, because the constructor could've run for a test in
    // kCppOnly mode, initializing those schemes only on the C++ side, and then
    // the CPSPI is reused for a subsequent test running in kRustOnly mode.
    // Ensure that default schemes are registered in whichever mode this test is
    // running in.
    //
    // TODO(crbug.com/493156320): Find a more comprehensive way to do
    // ChildProcessSecurityPolicy state reset in unit tests.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    policy->ResetRegisteredSchemesForTesting();

    // Also make sure the per-process ProcessStates don't leak across tests.
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->process_states_.GetSizeForTesting())
          << "ChildProcessSecurityPolicy should not be tracking any processes "
          << "at test startup.  Some other test probably forgot to call "
          << "Remove() at the end.";
    }
  }

  void TearDown() override {
    // TODO(crbug.com/493156320): Find a more comprehensive way to do
    // ChildProcessSecurityPolicy state reset in unit tests.
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    {
      base::AutoLock lock(policy->lock_);
      EXPECT_EQ(0u, policy->process_states_.GetSizeForTesting())
          << "ChildProcessSecurityPolicy should not be tracking any processes "
          << "at test shutdown.  Did you forget to call Remove() at the end of "
          << "a test?";
    }
    test_browser_client_.ClearSchemes();
    SetBrowserClientForTesting(old_browser_client_);
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
                               ChildProcessId child_id,
                               const base::FilePath& file,
                               int permissions) {
    p->GrantPermissionsForFile(child_id, file, permissions);
  }

  void CheckHasNoFileSystemPermission(ChildProcessSecurityPolicyImpl* p,
                                      int renderer_id,
                                      const std::string& filesystem_id) {
    EXPECT_FALSE(p->CanReadFileSystem(renderer_id, filesystem_id));
    EXPECT_FALSE(p->CanReadWriteFileSystem(renderer_id, filesystem_id));
    EXPECT_FALSE(p->CanCopyIntoFileSystem(renderer_id, filesystem_id));
    EXPECT_FALSE(p->CanDeleteFromFileSystem(renderer_id, filesystem_id));
  }

  void CheckHasNoFileSystemFilePermission(ChildProcessSecurityPolicyImpl* p,
                                          ChildProcessId process_id,
                                          int renderer_id,
                                          const base::FilePath& file,
                                          const storage::FileSystemURL& url) {
    EXPECT_FALSE(p->CanReadFile(process_id, file));
    EXPECT_FALSE(p->CanCreateReadWriteFile(renderer_id, file));
    EXPECT_FALSE(p->CanReadFileSystemFile(process_id, url));
    EXPECT_FALSE(p->CanWriteFileSystemFile(process_id, url));
    EXPECT_FALSE(p->CanCreateFileSystemFile(process_id, url));
    EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(process_id, url));
    EXPECT_FALSE(p->CanCopyIntoFileSystemFile(process_id, url));
    EXPECT_FALSE(p->CanDeleteFileSystemFile(process_id, url));

    auto handle = p->CreateHandle(process_id);
    EXPECT_FALSE(handle.CanReadFile(file));
    EXPECT_FALSE(handle.CanReadFileSystemFile(url));
  }

  BrowserContext* browser_context() { return &browser_context_; }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
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
    okpbd_feature_list_.InitAndDisableFeature(
        features::kOriginKeyedProcessesByDefault);
  }

 private:
  base::test::ScopedFeatureList okpbd_feature_list_;
};

TEST_P(ChildProcessSecurityPolicyTest, ChildID) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  MockRenderProcessHost process(browser_context());
  auto handle = p->CreateHandle(process.GetID());
  EXPECT_EQ(handle.child_id(), process.GetID());
}

TEST_P(ChildProcessSecurityPolicyTest, IsWebSafeSchemeTest) {
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

// Web-safe isolated schemes can be requested but not commit.
TEST_P(ChildProcessSecurityPolicyTest, IsWebSafeIsolatedSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Currently, no web-safe isolated schemes are registered within content/, but
  // embedders can register them.
  std::string web_safe_isolated_scheme("registered-web-safe-isolated-scheme");
  EXPECT_FALSE(p->IsWebSafeScheme(web_safe_isolated_scheme));
  p->RegisterWebSafeIsolatedScheme(web_safe_isolated_scheme);
  EXPECT_TRUE(p->IsWebSafeScheme(web_safe_isolated_scheme));
  EXPECT_FALSE(p->CanCommitSchemeInAnyProcess(web_safe_isolated_scheme));

  p->ClearRegisteredSchemeForTesting(web_safe_isolated_scheme);
}

TEST_P(ChildProcessSecurityPolicyTest, IsPseudoSchemeTest) {
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

TEST_P(ChildProcessSecurityPolicyTest, StandardSchemesTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  // Mark the process as used; otherwise, an unused and unlocked process is
  // allowed to commit any URL.
  process.SetIsUsed();

  auto handle = p->CreateHandle(process.GetID());

  // Safe to request, redirect or commit.
  const int renderer_id = process.GetDeprecatedID();
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("http://www.google.com/")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("https://www.paypal.com/")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("data:text/html,<b>Hi</b>")));
  EXPECT_TRUE(p->CanRequestURL(
      renderer_id, GURL("filesystem:http://localhost/temporary/a.gif")));
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
      EXPECT_FALSE(p->CanCommitURL(renderer_id, commit_url)) << commit_url;
    } else {
      EXPECT_TRUE(p->CanCommitURL(renderer_id, commit_url)) << commit_url;
    }
  }

  // A data URL can commit in any process.
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("data:text/html,<b>Hi</b>")));

  // Dangerous to request, commit, or set as origin header.
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GetWebUIURL("foo/bar")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id,
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
    EXPECT_FALSE(p->CanCommitURL(renderer_id, commit_url)) << commit_url;
  }
}

TEST_P(ChildProcessSecurityPolicyTest, BlobSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL localhost_url("http://localhost/");
  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(), localhost_url);

  EXPECT_TRUE(
      p->CanRequestURL(renderer_id, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(renderer_id, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanRequestURL(renderer_id, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("blob:NulL/some-guid?query")));
  EXPECT_FALSE(p->CanRequestURL(
      renderer_id, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(
      renderer_id, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id,
                                GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id,
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
      p->CanCommitURL(renderer_id, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("blob:null/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(renderer_id, GURL("blob:http://localhost/some-guid")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("blob:NulL/some-guid")));
  EXPECT_TRUE(
      p->CanCommitURL(renderer_id, GURL("blob:NulL/some-guid#fragment")));
  EXPECT_FALSE(p->CanCommitURL(
      renderer_id, GURL("blob:http://username@localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(
      renderer_id, GURL("blob:http://username     @localhost/some-guid")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("blob:blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("blob:some-guid")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id,
                               GURL("blob:filesystem:http://localhost/path")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id,
                               GURL("filesystem:blob:http://localhost/guid")));
}

TEST_P(ChildProcessSecurityPolicyTest, OpaqueBlobAndFileSystemSchemeTest) {
  const GURL opaque_blob_url("blob:null/some-guid");
  const GURL opaque_file_system_url(
      "filesystem:chrome-error://chromewebdata/temporary/a.gif");
  ASSERT_TRUE(opaque_blob_url.is_valid());
  ASSERT_TRUE(opaque_file_system_url.is_valid());
  ASSERT_TRUE(opaque_file_system_url.inner_url());
  ASSERT_TRUE(url::Origin::Create(opaque_blob_url).opaque());
  ASSERT_TRUE(url::Origin::Create(opaque_file_system_url).opaque());

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  EXPECT_TRUE(p->CanRequestURL(renderer_id, opaque_blob_url));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, opaque_blob_url));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, opaque_file_system_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, opaque_file_system_url));
}

TEST_P(ChildProcessSecurityPolicyTest, AboutTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("about:blank")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("about:blank")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("aBouT:blank")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("about:blank")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:BlAnK")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("aBouT:BlAnK")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("aBouT:blank")));

  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:srcdoc")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("about:srcdoc")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:SRCDOC")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:SRCDOC")));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:crash")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:cache")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:hang")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:cache")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:hang")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:version")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:cache")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:hang")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:version")));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:CrASh")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("aBoUt:version")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:CrASh")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("abOuT:cAChe")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("aBoUt:version")));

  // Requests for about: pages should be denied.
  p->GrantCommitURL(renderer_id, GURL("about:crash"));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("about:crash")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("about:crash")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("about:crash")));
}

TEST_P(ChildProcessSecurityPolicyTest, JavaScriptTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("javascript:alert('xss')")));
  p->GrantCommitURL(renderer_id, GURL("javascript:alert('xss')"));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("javascript:alert('xss')")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("javascript:alert('xss')")));
}

TEST_P(ChildProcessSecurityPolicyTest, RegisterWebSafeSchemeTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  // Currently, "asdf" is destined for ShellExecute, so it is allowed to be
  // requested but not committed.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("asdf:rockers")));

  // Once we register "asdf", we default to deny.
  RegisterTestScheme("asdf");
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("asdf:rockers")));

  // We can allow new schemes by adding them to the whitelist.
  p->RegisterWebSafeScheme("asdf");
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("asdf:rockers")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("asdf:rockers")));

  // At this point, `process` can still commit the URL because it's not locked
  // and unused; see the Citadel exception for unused processes in
  // ChildProcessSecurityPolicyImpl::PerformJailAndCitadelChecks().
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("asdf:rockers")));

  // Now mark the process as used.
  process.SetIsUsed();

  if (AreAllSitesIsolatedForTesting()) {
    // With site-per-process, a used process that isn't locked can't commit
    // the URL, since it requires a dedicated process.
    EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("asdf:rockers")));

    // A used process can't be locked to a site, so use a new process to check
    // that after locking the process, CanCommitURL returns true.
    MockRenderProcessHost locked_process(browser_context());
    LockProcessIfNeeded(locked_process.GetID(), browser_context(),
                        GURL("asdf:rockers"));
    EXPECT_TRUE(p->CanCommitURL(locked_process.GetDeprecatedID(),
                                GURL("asdf:rockers")));
  } else {
    // Without site-per-process, the URL doesn't require a dedicated process,
    // so even a used process that isn't locked can commit it.
    EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("asdf:rockers")));
  }

  // Cleanup.
  p->ClearRegisteredSchemeForTesting("asdf");
}

TEST_P(ChildProcessSecurityPolicyTest, CanServiceCommandsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL file_url("file:///etc/passwd");
  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context(), file_url);
  const int renderer_id = process->GetDeprecatedID();

  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("file:///etc/passwd")));
  p->GrantCommitURL(renderer_id, GURL("file:///etc/passwd"));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, GURL("file:///etc/passwd")));

  // A new process shouldn't inherit grants given to another process.
  MockRenderProcessHost new_process(browser_context());
  LockProcessIfNeeded(new_process.GetID(), browser_context());
  const int new_renderer_id = new_process.GetDeprecatedID();
  EXPECT_FALSE(p->CanRequestURL(new_renderer_id, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(new_renderer_id, GURL("file:///etc/passwd")));

  // Even if a stale ID were re-added after its process went away (which
  // shouldn't happen, since IDs aren't reused), it should start with fresh
  // state rather than resurrecting the old process's grants. Note that
  // destroying `process` calls Remove(), but its old state may still be
  // pending removal until a task runs on the IO thread.
  const ChildProcessId stale_id = process->GetID();
  process.reset();
  p->AddForTesting(stale_id, browser_context());
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("file:///etc/passwd")));
  p->Remove(stale_id);
}

TEST_P(ChildProcessSecurityPolicyTest, ViewSource) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  // Child processes cannot request view source URLs.
  EXPECT_FALSE(p->CanRequestURL(renderer_id,
                                GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(
      p->CanRequestURL(renderer_id, GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRequestURL(
      renderer_id, GURL("view-source:view-source:http://www.google.com/")));

  // Child processes cannot be redirected to view source URLs.
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(
      GURL("view-source:view-source:http://www.google.com/")));

  // View source URLs don't actually commit; the renderer is put into view
  // source mode, and the inner URL commits.
  EXPECT_FALSE(
      p->CanCommitURL(renderer_id, GURL("view-source:http://www.google.com/")));
  EXPECT_FALSE(
      p->CanCommitURL(renderer_id, GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(
      renderer_id, GURL("view-source:view-source:http://www.google.com/")));

  p->GrantCommitURL(renderer_id, GURL("view-source:file:///etc/passwd"));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_TRUE(p->CanRedirectToURL(GURL("file:///etc/passwd")));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, GURL("file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanRequestURL(renderer_id, GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(p->CanRedirectToURL(GURL("view-source:file:///etc/passwd")));
  EXPECT_FALSE(
      p->CanCommitURL(renderer_id, GURL("view-source:file:///etc/passwd")));
}

TEST_P(ChildProcessSecurityPolicyTest, GoogleChromeScheme) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  MockRenderProcessHost process(browser_context());
  LockProcessIfNeeded(process.GetID(), browser_context());
  const int renderer_id = process.GetDeprecatedID();

  GURL test_url("googlechrome://whatever");

  EXPECT_FALSE(p->CanRequestURL(renderer_id, test_url));
  EXPECT_FALSE(p->CanRedirectToURL(test_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, test_url));
}

TEST_P(ChildProcessSecurityPolicyTest, GrantCommitURLToNonStandardScheme) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const GURL url("httpxml://awesome");
  const GURL url2("httpxml://also-awesome");

  ASSERT_TRUE(url::Origin::Create(url).opaque());
  ASSERT_TRUE(url::Origin::Create(url2).opaque());
  RegisterTestScheme("httpxml");

  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(), url);

  EXPECT_FALSE(p->CanRequestURL(renderer_id, url));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url2));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanRedirectToURL(url2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url2));

  // GrantCommitURL with a non-standard scheme should grant commit access to the
  // entire scheme.
  p->GrantCommitURL(renderer_id, url);

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url2));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanRedirectToURL(url2));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url2));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(ChildProcessSecurityPolicyTest, ExternalFile_Normalization) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL base_url("externalfile:foo.pdf");
  GURL url_with_ref("externalfile:foo.pdf#page=1");
  GURL url_with_query("externalfile:foo.pdf?print=true");
  GURL url_with_both("externalfile:foo.pdf?print=true#page=1");
  GURL completely_different_url("externalfile:bar.pdf");

  // Verify that externalfile is registered as a handled scheme on ChromeOS.
  ASSERT_TRUE(GetContentClientForTesting()->browser()->IsHandledURL(base_url));

  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(), base_url);

  // Grant request access to the base URL.
  p->GrantRequestOfExternalFileUrl(process.GetID(), base_url);

  // Verify that the base URL is allowed.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, base_url));

  // Verify that variations with query and ref are also allowed,
  // proving that the URLs are being properly normalized under the hood.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_with_ref));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_with_query));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_with_both));

  // Verify that normalization doesn't accidentally grant access to other files.
  EXPECT_FALSE(p->CanRequestURL(renderer_id, completely_different_url));

  // Grant commit access using a URL that contains a query and ref.
  p->GrantCommitOfExternalFileUrl(process.GetID(), url_with_both);

  // Verify that normalization allows the base URL to be committed,
  // even though the grant was issued using a complex URL.
  EXPECT_TRUE(p->CanCommitURL(renderer_id, base_url));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(ChildProcessSecurityPolicyTest, ExternalFile) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL externalfile1("externalfile:foo.png");
  GURL externalfile2("externalfile:bar.png");

  // Verify that externalfile is registered as a handled scheme on ChromeOS.
  ASSERT_TRUE(
      GetContentClientForTesting()->browser()->IsHandledURL(externalfile1));

  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(), externalfile2);

  // Initially, neither can be requested or committed.
  EXPECT_FALSE(p->CanRequestURL(renderer_id, externalfile1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, externalfile1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, externalfile2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, externalfile2));

  // Grant ONLY request access to externalfile1.
  p->GrantRequestOfExternalFileUrl(process.GetID(), externalfile1);

  // Verify request is granted, but commit is strictly denied.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, externalfile1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, externalfile1));

  // externalfile2 should remain entirely unaffected.
  EXPECT_FALSE(p->CanRequestURL(renderer_id, externalfile2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, externalfile2));

  // Grant ONLY commit access to externalfile2.
  p->GrantCommitOfExternalFileUrl(process.GetID(), externalfile2);

  // Verify commit is granted and request is also granted.
  EXPECT_TRUE(p->CanCommitURL(renderer_id, externalfile2));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, externalfile2));
}
#endif

TEST_P(ChildProcessSecurityPolicyTest, SpecificFile) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL icon_url("file:///tmp/foo.png");
  base::FilePath icon_path;
  ASSERT_TRUE(net::FileURLToFilePath(icon_url, &icon_path));
  GURL sensitive_url("file:///etc/passwd");

  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(), sensitive_url);

  EXPECT_FALSE(p->CanRequestURL(renderer_id, icon_url));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, icon_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, sensitive_url));

  p->GrantRequestOfSpecificFile(process.GetID(), icon_path);
  EXPECT_TRUE(p->CanRequestURL(renderer_id, icon_url));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, icon_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, sensitive_url));

  // Verify that RevokeAllPermissionsForFile() revokes the permission granted
  // by GrantRequestOfSpecificFile().
  p->RevokeAllPermissionsForFile(process.GetID(), icon_path);
  EXPECT_FALSE(p->CanRequestURL(renderer_id, icon_url));
  p->GrantRequestOfSpecificFile(process.GetID(), icon_path);
  EXPECT_TRUE(p->CanRequestURL(renderer_id, icon_url));

  // Verify that GrantRequestOfSpecificFile() and RevokeAllPermissionsForFile()
  // ignore trailing separators.
  p->RevokeAllPermissionsForFile(process.GetID(),
                                 icon_path.AsEndingWithSeparator());
  EXPECT_FALSE(p->CanRequestURL(renderer_id, icon_url));
  p->GrantRequestOfSpecificFile(process.GetID(),
                                icon_path.AsEndingWithSeparator());
  EXPECT_TRUE(p->CanRequestURL(renderer_id, icon_url));

  p->GrantCommitURL(renderer_id, icon_url);
  EXPECT_TRUE(p->CanRequestURL(renderer_id, icon_url));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, sensitive_url));
  EXPECT_TRUE(p->CanRedirectToURL(icon_url));
  EXPECT_TRUE(p->CanRedirectToURL(sensitive_url));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, icon_url));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, sensitive_url));
}

TEST_P(ChildProcessSecurityPolicyTest, ContentUri) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL content_uri("content://authority/foo.png");
  GURL content_uri_sensitive("content://authority/bar.jpg");

  MockRenderProcessHost process(browser_context());
  const int renderer_id = process.GetDeprecatedID();
  LockProcessIfNeeded(process.GetID(), browser_context(),
                      content_uri_sensitive);

#if BUILDFLAG(IS_ANDROID)
  // Since android handles content:// URLs, CanRequestURL() is false for a URL
  // which was not registered with GrantRequestOfSpecificFile().
  EXPECT_FALSE(p->CanRequestURL(renderer_id, content_uri));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, content_uri_sensitive));
#else
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri_sensitive));
#endif
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, content_uri));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, content_uri_sensitive));

  p->GrantRequestOfSpecificFile(
      process.GetID(),
      base::FilePath::FromUTF8Unsafe(content_uri.possibly_invalid_spec()));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(p->CanRequestURL(renderer_id, content_uri_sensitive));
#else
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri_sensitive));
#endif
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, content_uri));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, content_uri_sensitive));

  p->GrantCommitURL(renderer_id, content_uri);
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, content_uri_sensitive));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri));
  EXPECT_TRUE(p->CanRedirectToURL(content_uri_sensitive));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, content_uri));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, content_uri_sensitive));
}

TEST_P(ChildProcessSecurityPolicyTest, FileSystemGrantsTest) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context());
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();
  std::string read_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "read_filesystem", base::FilePath());
  std::string read_write_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "read_write_filesystem",
          base::FilePath());
  std::string copy_into_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "copy_into_filesystem",
          base::FilePath());
  std::string delete_from_id =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypeTest, "delete_from_filesystem",
          base::FilePath());

  // Test initially having no permissions.
  CheckHasNoFileSystemPermission(p, renderer_id, read_id);
  CheckHasNoFileSystemPermission(p, renderer_id, read_write_id);
  CheckHasNoFileSystemPermission(p, renderer_id, copy_into_id);
  CheckHasNoFileSystemPermission(p, renderer_id, delete_from_id);

  // Testing varying combinations of grants and checks.
  p->GrantReadFileSystem(renderer_id, read_id);
  EXPECT_TRUE(p->CanReadFileSystem(renderer_id, read_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(renderer_id, read_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(renderer_id, read_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(renderer_id, read_id));

  p->GrantReadFileSystem(renderer_id, read_write_id);
  p->GrantWriteFileSystem(renderer_id, read_write_id);
  EXPECT_TRUE(p->CanReadFileSystem(renderer_id, read_write_id));
  EXPECT_TRUE(p->CanReadWriteFileSystem(renderer_id, read_write_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(renderer_id, read_write_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(renderer_id, read_write_id));

  p->GrantCopyIntoFileSystem(renderer_id, copy_into_id);
  EXPECT_FALSE(p->CanReadFileSystem(renderer_id, copy_into_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(renderer_id, copy_into_id));
  EXPECT_TRUE(p->CanCopyIntoFileSystem(renderer_id, copy_into_id));
  EXPECT_FALSE(p->CanDeleteFromFileSystem(renderer_id, copy_into_id));

  p->GrantDeleteFromFileSystem(renderer_id, delete_from_id);
  EXPECT_FALSE(p->CanReadFileSystem(renderer_id, delete_from_id));
  EXPECT_FALSE(p->CanReadWriteFileSystem(renderer_id, delete_from_id));
  EXPECT_FALSE(p->CanCopyIntoFileSystem(renderer_id, delete_from_id));
  EXPECT_TRUE(p->CanDeleteFromFileSystem(renderer_id, delete_from_id));

  // Test revoke permissions on process removal.
  process.reset();
  CheckHasNoFileSystemPermission(p, renderer_id, read_id);
  CheckHasNoFileSystemPermission(p, renderer_id, read_write_id);
  CheckHasNoFileSystemPermission(p, renderer_id, copy_into_id);
  CheckHasNoFileSystemPermission(p, renderer_id, delete_from_id);

  // Even if a stale ID were re-added after its process went away (which
  // shouldn't happen, since IDs aren't reused), it should start with fresh
  // state rather than resurrecting the old process's permissions.
  p->AddForTesting(process_id, browser_context());
  CheckHasNoFileSystemPermission(p, renderer_id, read_id);
  CheckHasNoFileSystemPermission(p, renderer_id, read_write_id);
  CheckHasNoFileSystemPermission(p, renderer_id, copy_into_id);
  CheckHasNoFileSystemPermission(p, renderer_id, delete_from_id);

  // Cleanup.
  p->Remove(process_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(read_write_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(copy_into_id);
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(delete_from_id);
}

TEST_P(ChildProcessSecurityPolicyTest, FilePermissionGrantingAndRevoking) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->RegisterFileSystemPermissionPolicy(
      storage::kFileSystemTypeTest,
      storage::FILE_PERMISSION_USE_FILE_PERMISSION);

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();
  LockProcessIfNeeded(process_id, browser_context(), GURL("http://foo/"));
  p->AddCommittedOrigin(renderer_id, url::Origin::Create(GURL("http://foo/")));

  base::FilePath file(TEST_PATH("/dir/testfile"));
  file = file.NormalizePathSeparators();
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFromStringForTesting("http://foo/"),
      storage::kFileSystemTypeTest, file);

  // Test initially having no permissions.
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);

  // Testing every combination of permissions granting and revoking.
  p->GrantReadFile(process_id, file);
  EXPECT_TRUE(p->CanReadFile(process_id, file));
  EXPECT_FALSE(p->CanCreateReadWriteFile(renderer_id, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanWriteFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCreateFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(process_id, url));
  p->RevokeAllPermissionsForFile(process_id, file);
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);

  p->GrantCreateReadWriteFile(renderer_id, file);
  EXPECT_TRUE(p->CanReadFile(process_id, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(renderer_id, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(process_id, url));
  p->RevokeAllPermissionsForFile(process_id, file);
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);

  // Test revoke permissions on process removal.
  p->GrantCreateReadWriteFile(renderer_id, file);
  EXPECT_TRUE(p->CanReadFile(process_id, file));
  EXPECT_TRUE(p->CanCreateReadWriteFile(renderer_id, file));
  EXPECT_TRUE(p->CanReadFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanWriteFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCreateFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCreateReadWriteFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanCopyIntoFileSystemFile(process_id, url));
  EXPECT_TRUE(p->CanDeleteFileSystemFile(process_id, url));
  process.reset();
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);

  // Even if a stale ID were re-added after its process went away (which
  // shouldn't happen, since IDs aren't reused), it should start with fresh
  // state rather than resurrecting the old process's permissions.
  p->AddForTesting(process_id, browser_context());
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);
  LockProcessIfNeeded(process_id, browser_context(), GURL("http://foo/"));
  p->AddCommittedOrigin(renderer_id, url::Origin::Create(GURL("http://foo/")));
  CheckHasNoFileSystemFilePermission(p, process_id, renderer_id, file, url);

  // Cleanup.
  p->Remove(process_id);
}

TEST_P(ChildProcessSecurityPolicyTest, FilePermissions) {
  base::FilePath granted_file = base::FilePath(TEST_PATH("/home/joe"));
  base::FilePath sibling_file = base::FilePath(TEST_PATH("/home/bob"));
  base::FilePath child_file = base::FilePath(TEST_PATH("/home/joe/file"));
  base::FilePath parent_file = base::FilePath(TEST_PATH("/home"));
  base::FilePath parent_slash_file = base::FilePath(TEST_PATH("/home/"));
  base::FilePath child_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/././file"));
  base::FilePath child_traversal2 =
      base::FilePath(TEST_PATH("/home/joe/file/../otherfile"));
  base::FilePath evil_traversal1 =
      base::FilePath(TEST_PATH("/home/joe/../../etc/passwd"));
  base::FilePath evil_traversal2 =
      base::FilePath(TEST_PATH("/home/joe/./.././../etc/passwd"));
  base::FilePath self_traversal =
      base::FilePath(TEST_PATH("/home/joe/../joe/file"));
  base::FilePath relative_file = base::FilePath(FILE_PATH_LITERAL("home/joe"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Grant permissions for a file.
  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context());
  const ChildProcessId process_id = process->GetID();
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_OPEN));

  GrantPermissionsForFile(p, process_id, granted_file,
                          base::File::FLAG_OPEN |
                              base::File::FLAG_OPEN_TRUNCATED |
                              base::File::FLAG_READ | base::File::FLAG_WRITE);
  EXPECT_TRUE(p->HasPermissionsForFile(
      process_id, granted_file,
      base::File::FLAG_OPEN | base::File::FLAG_OPEN_TRUNCATED |
          base::File::FLAG_READ | base::File::FLAG_WRITE));
  EXPECT_TRUE(p->HasPermissionsForFile(
      process_id, granted_file, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_CREATE));
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file, 0));
  EXPECT_FALSE(p->HasPermissionsForFile(
      process_id, granted_file,
      base::File::FLAG_CREATE | base::File::FLAG_OPEN_TRUNCATED |
          base::File::FLAG_READ | base::File::FLAG_WRITE));
  EXPECT_FALSE(p->HasPermissionsForFile(
      process_id, sibling_file, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(
      process_id, parent_file, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_TRUE(p->HasPermissionsForFile(
      process_id, child_file, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_TRUE(
      p->HasPermissionsForFile(process_id, child_traversal1,
                               base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_TRUE(
      p->HasPermissionsForFile(process_id, child_traversal2,
                               base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_FALSE(
      p->HasPermissionsForFile(process_id, evil_traversal1,
                               base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_FALSE(
      p->HasPermissionsForFile(process_id, evil_traversal2,
                               base::File::FLAG_OPEN | base::File::FLAG_READ));
  // CPSP doesn't allow this case for the sake of simplicity.
  EXPECT_FALSE(
      p->HasPermissionsForFile(process_id, self_traversal,
                               base::File::FLAG_OPEN | base::File::FLAG_READ));
  process.reset();

  // The rest of this test re-adds the same (now stale) ID to start over with a
  // clean state. This shouldn't happen in production, since IDs aren't reused,
  // but a re-added ID should start with fresh state rather than resurrecting
  // the old process's permissions.

  // Grant permissions for the directory the file is in.
  p->AddForTesting(process_id, browser_context());
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, process_id, parent_file,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(process_id, granted_file,
                                       base::File::FLAG_OPEN));
  EXPECT_FALSE(
      p->HasPermissionsForFile(process_id, granted_file,
                               base::File::FLAG_READ | base::File::FLAG_WRITE));
  p->Remove(process_id);

  // Grant permissions for the directory the file is in (with trailing '/').
  p->AddForTesting(process_id, browser_context());
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_OPEN));
  GrantPermissionsForFile(p, process_id, parent_slash_file,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(p->HasPermissionsForFile(process_id, granted_file,
                                       base::File::FLAG_OPEN));
  EXPECT_FALSE(
      p->HasPermissionsForFile(process_id, granted_file,
                               base::File::FLAG_READ | base::File::FLAG_WRITE));

  // Grant permissions for the file (should overwrite the permissions granted
  // for the directory).
  GrantPermissionsForFile(p, process_id, granted_file,
                          base::File::FLAG_WIN_TEMPORARY);
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_OPEN));
  EXPECT_TRUE(p->HasPermissionsForFile(process_id, granted_file,
                                       base::File::FLAG_WIN_TEMPORARY));

  // Revoke all permissions for the file (it should inherit its permissions
  // from the directory again).
  p->RevokeAllPermissionsForFile(process_id, granted_file);
  EXPECT_TRUE(p->HasPermissionsForFile(
      process_id, granted_file, base::File::FLAG_OPEN | base::File::FLAG_READ));
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, granted_file,
                                        base::File::FLAG_WIN_TEMPORARY));
  p->Remove(process_id);

  p->AddForTesting(process_id, browser_context());
  GrantPermissionsForFile(p, process_id, relative_file, base::File::FLAG_OPEN);
  EXPECT_FALSE(p->HasPermissionsForFile(process_id, relative_file,
                                        base::File::FLAG_OPEN));
  p->Remove(process_id);
}

TEST_P(ChildProcessSecurityPolicyTest, CanServiceWebUIBindings) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const GURL url(GetWebUIURL("thumb/http://www.google.com/"));
  const GURL other_url(GetWebUIURL("not-thumb/"));
  const url::Origin origin = url::Origin::Create(url);

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();
  LockProcessIfNeeded(process_id, browser_context(), url);

  EXPECT_FALSE(p->HasWebUIBindings(process_id));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, other_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, other_url));
  EXPECT_TRUE(p->CanRedirectToURL(other_url));

  p->GrantWebUIBindings(process_id);

  EXPECT_TRUE(p->HasWebUIBindings(process_id));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, other_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, other_url));
  EXPECT_TRUE(p->CanRedirectToURL(other_url));

  p->GrantCommitOrigin(renderer_id, origin);

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));

  EXPECT_FALSE(p->CanRequestURL(renderer_id, other_url));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, other_url));
  EXPECT_TRUE(p->CanRedirectToURL(other_url));

  // Destroying the process removes it from ChildProcessSecurityPolicy.
  process.reset();

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(p->HasWebUIBindings(process_id));
}

TEST_P(ChildProcessSecurityPolicyTest, CanServiceWebUIBindingsWithHandle) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const GURL url(GetWebUIURL("thumb/http://www.google.com/"));

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const ChildProcessId process_id = process->GetID();
  LockProcessIfNeeded(process_id, browser_context(), url);
  EXPECT_FALSE(p->HasWebUIBindings(process_id));

  p->GrantWebUIBindings(process_id);
  EXPECT_TRUE(p->HasWebUIBindings(process_id));

  // Creating a Handle should make the ProcessState live beyond Remove(), which
  // happens when the RenderProcessHost is destroyed.
  auto handle = p->CreateHandle(process_id);
  process.reset();
  EXPECT_TRUE(p->HasWebUIBindings(process_id));

  // Invalidate `handle` so it does not preserve process state anymore.
  handle = ChildProcessSecurityPolicyImpl::Handle();

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(p->HasWebUIBindings(process_id));
}

TEST_P(ChildProcessSecurityPolicyTest, OriginCheckExemptionForWebView) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const url::Origin file_origin = url::Origin::Create(GURL("file:///"));
  const url::Origin opaque_origin =
      url::Origin::Create(GURL("http://foo.com")).DeriveNewOpaqueOrigin();
  const url::Origin other_opaque_origin =
      url::Origin::Create(GURL("http://foo.com")).DeriveNewOpaqueOrigin();

  // Queries and grants for an unregistered process should be safe no-ops.
  const ChildProcessId unregistered_process_id =
      ChildProcessHost::GenerateChildProcessUniqueId();
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(unregistered_process_id,
                                                    file_origin));
  p->GrantOriginCheckExemptionForWebView(unregistered_process_id, file_origin);
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(unregistered_process_id,
                                                    file_origin));

  // A newly registered process has no origin check exemptions.
  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context());
  const ChildProcessId process_id = process->GetID();
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(process_id, file_origin));
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(process_id, opaque_origin));

  // Granting an exemption for a file origin should exempt only that origin.
  p->GrantOriginCheckExemptionForWebView(process_id, file_origin);
  EXPECT_TRUE(p->HasOriginCheckExemptionForWebView(process_id, file_origin));
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(process_id, opaque_origin));

  // Granting an exemption for an opaque origin should exempt that origin, but
  // not a distinct opaque origin with the same precursor.
  p->GrantOriginCheckExemptionForWebView(process_id, opaque_origin);
  EXPECT_TRUE(p->HasOriginCheckExemptionForWebView(process_id, opaque_origin));
  EXPECT_FALSE(
      p->HasOriginCheckExemptionForWebView(process_id, other_opaque_origin));

  // Creating a Handle should make the ProcessState live beyond Remove(), which
  // happens when the RenderProcessHost is destroyed.
  auto handle = p->CreateHandle(process_id);
  process.reset();
  EXPECT_TRUE(p->HasOriginCheckExemptionForWebView(process_id, file_origin));
  EXPECT_TRUE(p->HasOriginCheckExemptionForWebView(process_id, opaque_origin));

  // Grants after Remove should be ignored even while a Handle exists.
  p->GrantOriginCheckExemptionForWebView(process_id, other_opaque_origin);
  EXPECT_FALSE(
      p->HasOriginCheckExemptionForWebView(process_id, other_opaque_origin));

  // Invalidate `handle` so it does not preserve process state anymore.
  handle = ChildProcessSecurityPolicyImpl::Handle();

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  // Once removal has completed, the process should no longer have any
  // exemptions.
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(process_id, file_origin));
  EXPECT_FALSE(p->HasOriginCheckExemptionForWebView(process_id, opaque_origin));
}

TEST_P(ChildProcessSecurityPolicyTest, RemoveRace) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("file:///etc/passwd");
  base::FilePath file(TEST_PATH("/etc/passwd"));

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context());
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();

  p->GrantCommitURL(renderer_id, url);
  p->GrantReadFile(process_id, file);
  p->GrantWebUIBindings(process_id);

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_TRUE(p->CanReadFile(process_id, file));
  EXPECT_TRUE(p->HasWebUIBindings(process_id));

  process.reset();

  // Renderers are added and removed on the UI thread, but the policy can be
  // queried on the IO thread.  The ChildProcessSecurityPolicy needs to be
  // prepared to answer policy questions about renderers who no longer exist.

  // In this case, we default to secure behavior.
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url));
  EXPECT_TRUE(p->CanRedirectToURL(url));
  EXPECT_FALSE(p->CanReadFile(process_id, file));
  EXPECT_FALSE(p->HasWebUIBindings(process_id));
}

TEST_P(ChildProcessSecurityPolicyTest, HandleDuplicate) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url("file:///etc/passwd");

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const ChildProcessId process_id = process->GetID();
  LockProcessIfNeeded(process_id, browser_context(), url);
  p->AddCommittedOrigin(process->GetDeprecatedID(), url::Origin::Create(url));

  auto handle = p->CreateHandle(process_id);

  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(url)));

  // Verify that a valid duplicate can be created and allows access.
  auto duplicate_handle = handle.Duplicate();
  EXPECT_TRUE(duplicate_handle.is_valid());
  EXPECT_TRUE(
      duplicate_handle.CanAccessDataForOrigin(url::Origin::Create(url)));

  // Destroying the process calls Remove().
  process.reset();

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
  EXPECT_FALSE(p->CreateHandle(process_id).is_valid());

  // Invalidate the original Handle and verify that the duplicates still work.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)));
  EXPECT_TRUE(
      duplicate_handle.CanAccessDataForOrigin(url::Origin::Create(url)));
  EXPECT_TRUE(
      duplicate_handle2.CanAccessDataForOrigin(url::Origin::Create(url)));
}

TEST_P(ChildProcessSecurityPolicyTest, CanAccessDataForOrigin_URL) {
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
  const ChildProcessId unregistered_process_id =
      ChildProcessHost::GenerateChildProcessUniqueId();
  auto handle = p->CreateHandle(unregistered_process_id);
  for (auto url : kAllTestUrls) {
    // TODO(crbug.com/379869738): Remove GetUnsafeValue().
    EXPECT_FALSE(p->CanAccessDataForOrigin(
        unregistered_process_id.GetUnsafeValue(), url::Origin::Create(url)))
        << url;
    EXPECT_FALSE(
        handle.CanAccessDataForOrigin(url::Origin::Create(bar_http_url)))
        << url;
  }

  TestBrowserContext browser_context;
  auto process = std::make_unique<MockRenderProcessHost>(&browser_context);
  LockProcessIfNeeded(process->GetID(), &browser_context);
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();

  // Replace the old invalid handle with a new valid handle.
  handle = p->CreateHandle(process_id);

  // Verify unlocked origin permissions.
  for (auto url : kAllTestUrls) {
    // An unlocked process cannot access URLs below (because with
    // site-per-process all the URLs need to be isolated). Since
    // CanAccessDataForOrigin enforcement relies on committed origin tracking,
    // these should fail even without site isolation, since the process hasn't
    // committed any origins at this point.
    EXPECT_FALSE(
        p->CanAccessDataForOrigin(renderer_id, url::Origin::Create(url)))
        << url;
    EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)))
        << url;
  }

  // Isolate |foo_http_url| so we can't get a default SiteInstance.
  p->AddFutureIsolatedOrigins({url::Origin::Create(foo_http_url)},
                              IsolatedOriginSource::TEST, &browser_context);

  // Lock process to |foo_http_url| origin and pretend that it's been committed.
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&browser_context, foo_http_url);
  EXPECT_FALSE(foo_instance->IsDefaultSiteInstance());
  LockProcessIfNeeded(process_id, &browser_context, foo_http_url);
  p->AddCommittedOrigin(renderer_id, url::Origin::Create(foo_http_url));

  // Verify that file access is no longer allowed.
  EXPECT_FALSE(
      p->CanAccessDataForOrigin(renderer_id, url::Origin::Create(file_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id,
                                        url::Origin::Create(foo_http_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id,
                                        url::Origin::Create(foo_blob_url)));
  EXPECT_TRUE(p->CanAccessDataForOrigin(
      renderer_id, url::Origin::Create(foo_filesystem_url)));
  EXPECT_FALSE(p->CanAccessDataForOrigin(renderer_id,
                                         url::Origin::Create(bar_http_url)));
  EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(file_url)));
  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(foo_http_url)));
  EXPECT_TRUE(handle.CanAccessDataForOrigin(url::Origin::Create(foo_blob_url)));
  EXPECT_TRUE(
      handle.CanAccessDataForOrigin(url::Origin::Create(foo_filesystem_url)));
  EXPECT_FALSE(
      handle.CanAccessDataForOrigin(url::Origin::Create(bar_http_url)));

  // Invalidate handle so it does not preserve process state beyond Remove().
  handle = ChildProcessSecurityPolicyImpl::Handle();

  // Destroying the process calls Remove().
  process.reset();

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify invalid ID is rejected now that Remove() has completed.
  for (auto url : kAllTestUrls) {
    EXPECT_FALSE(
        p->CanAccessDataForOrigin(renderer_id, url::Origin::Create(url)))
        << url;
    EXPECT_FALSE(handle.CanAccessDataForOrigin(url::Origin::Create(url)))
        << url;
  }
}

TEST_P(ChildProcessSecurityPolicyTest, CanAccessDataForOrigin_Origin) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  const std::vector<const char*> foo_urls = {
      "http://foo.com/index.html",
      "blob:http://foo.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://foo.com/temporary/test.html",
      // TODO(acolwell): data: should be in |non_foo_urls| in the long-term.
      "data:text/html,Hello!"};

  const std::vector<const char*> non_foo_urls = {
      "file:///etc/passwd", "http://bar.com/index.html",
      "blob:http://bar.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://bar.com/temporary/test.html",
      // foo.com with a different scheme not considered equal.
      "https://foo.com/index.html",
      "blob:https://foo.com/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:https://foo.com/temporary/test.html"};

  const std::vector<const char*> foo_urls_with_port_mismatch = {
      "http://foo.com:1234/index.html",
      "blob:http://foo.com:1234/43d75119-d7af-4471-a293-07c6b3d7e61a",
      "filesystem:http://foo.com:1234/temporary/test.html"};

  std::vector<url::Origin> foo_origins;
  std::vector<url::Origin> non_foo_origins;
  std::vector<url::Origin> all_origins;
  for (auto* url : foo_urls) {
    auto origin = url::Origin::Create(GURL(url));
    foo_origins.push_back(origin);
    all_origins.push_back(origin);
  }
  auto foo_origin = url::Origin::Create(GURL("http://foo.com"));

  // TODO(crbug.com/40148776): Committed origin enforcements should stop
  // allowing a non-opaque committed origin to match an opaque origin, even if
  // the latter's precursor matches. See TODO in
  // ProcessState::MatchesCommittedOrigin().
  auto opaque_with_foo_precursor = foo_origin.DeriveNewOpaqueOrigin();
  foo_origins.push_back(opaque_with_foo_precursor);
  all_origins.push_back(opaque_with_foo_precursor);

  // List-of-committed-origins enforcements perform stricter checks in cases
  // where origins differ only in port.
  for (auto* url : foo_urls_with_port_mismatch) {
    auto origin = url::Origin::Create(GURL(url));
    non_foo_origins.push_back(origin);
    all_origins.push_back(origin);
  }

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
  const ChildProcessId unregistered_process_id =
      ChildProcessHost::GenerateChildProcessUniqueId();
  for (const auto& origin : all_origins) {
    // TODO(crbug.com/379869738): Remove GetUnsafeValue().
    EXPECT_FALSE(p->CanAccessDataForOrigin(
        unregistered_process_id.GetUnsafeValue(), origin))
        << origin;
  }

  TestBrowserContext browser_context;
  auto process = std::make_unique<MockRenderProcessHost>(&browser_context);
  LockProcessIfNeeded(process->GetID(), &browser_context);
  const int renderer_id = process->GetDeprecatedID();

  // Verify unlocked process permissions.
  for (const auto& origin : all_origins) {
    // An unlocked process cannot access URLs below (because with
    // site-per-process all the URLs need to be isolated). Since
    // CanAccessDataForOrigin enforcement uses committed origin tracking, then
    // these should fail even without site isolation, since the process hasn't
    // committed any origins at this point. The only exception is for opaque
    // origins with no precursor, which are currently allowed; see TODO in
    // ChildProcessSecurityPolicyImpl::CanAccessOrigin().
    if (origin.opaque() &&
        !origin.GetTupleOrPrecursorTupleIfOpaque().IsValid()) {
      EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id, origin)) << origin;
    } else {
      EXPECT_FALSE(p->CanAccessDataForOrigin(renderer_id, origin)) << origin;
    }
  }

  // Isolate |foo_origin| so we can't get a default SiteInstance.
  p->AddFutureIsolatedOrigins({foo_origin}, IsolatedOriginSource::TEST,
                              &browser_context);

  // Lock process to |foo_origin| origin and pretend that it's been committed.
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&browser_context, foo_origin.GetURL());
  EXPECT_FALSE(foo_instance->IsDefaultSiteInstance());
  LockProcessIfNeeded(process->GetID(), &browser_context, foo_origin.GetURL());
  p->AddCommittedOrigin(renderer_id, foo_origin);

  // Verify that access is no longer allowed for origins that are not associated
  // with foo.com.
  for (const auto& origin : foo_origins) {
    EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id, origin)) << origin;
  }

  for (const auto& origin : non_foo_origins) {
    EXPECT_FALSE(p->CanAccessDataForOrigin(renderer_id, origin)) << origin;
  }

  // Destroying the process calls Remove().
  process.reset();

  // Post a task to the IO loop that then posts a task to the UI loop.
  // This should cause the |run_loop| to return after the removal has completed.
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                              run_loop.QuitClosure());
  run_loop.Run();

  // Verify invalid ID is rejected now that Remove() has completed.
  for (const auto& origin : all_origins) {
    EXPECT_FALSE(p->CanAccessDataForOrigin(renderer_id, origin)) << origin;
  }
}

// Tests that queries for Midi permissions work after RenderProcessHost removal
// until the corresponding Handles are gone. See https://crbug.com/471021577.
TEST_P(ChildProcessSecurityPolicyTest, MidiAfterProcessRemoval) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  LockProcessIfNeeded(process->GetID(), browser_context());
  const ChildProcessId process_id = process->GetID();
  const int renderer_id = process->GetDeprecatedID();
  auto handle = p->CreateHandle(process_id);

  p->GrantSendMidiMessage(renderer_id);
  EXPECT_TRUE(p->CanSendMidiMessage(process_id));
  EXPECT_FALSE(p->CanSendMidiSysExMessage(process_id));
  p->GrantSendMidiSysExMessage(renderer_id);
  EXPECT_TRUE(p->CanSendMidiMessage(process_id));
  EXPECT_TRUE(p->CanSendMidiSysExMessage(process_id));

  // Destroy the RenderProcessHost while Handles still exist.
  process.reset();

  // Queries should still succeed while the Handle exists.
  EXPECT_TRUE(p->CanSendMidiMessage(process_id));
  EXPECT_TRUE(p->CanSendMidiSysExMessage(process_id));

  // Queries should no longer succeed after the Handle is invalidated.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(p->CanSendMidiMessage(process_id));
  EXPECT_FALSE(p->CanSendMidiSysExMessage(process_id));
}

// Exercise the basic functionality of how MatchesCommittedOrigin() matches URLs
// against origins that have committed in a process. This test simulates an
// unlocked process that may commit origins from different sites (e.g., in a
// default SiteInstance on Android).
TEST_P(ChildProcessSecurityPolicyTest, MatchesCommittedOrigin) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext browser_context;
  MockRenderProcessHost process(&browser_context);
  LockProcessIfNeeded(process.GetID(), &browser_context);
  const ChildProcessId process_id = process.GetID();
  const int renderer_id = process.GetDeprecatedID();

  auto foo_origin = url::Origin::Create(GURL("http://foo.com"));

  // Helper wrapper for checking ProcessState::MatchesCommittedOrigin().
  auto matches_committed_origin = [&](const std::string& url,
                                      bool url_is_for_precursor_origin) {
    return p->MatchesCommittedOriginForTesting(process_id, GURL(url),
                                               url_is_for_precursor_origin);
  };

  EXPECT_FALSE(matches_committed_origin("http://foo.com/", false));

  // Pretend that the renderer has committed http://foo.com/.
  p->AddCommittedOrigin(renderer_id, foo_origin);

  // These URLs should all match the http://foo.com/ origin.
  EXPECT_TRUE(matches_committed_origin("http://foo.com/", false));
  EXPECT_TRUE(matches_committed_origin("http://foo.com/foo/bar", false));
  EXPECT_TRUE(matches_committed_origin("blob:http://foo.com/uuid", false));
  EXPECT_TRUE(
      matches_committed_origin("filesystem:http://foo.com/some/path", false));

  // These URLs should not match http://foo.com/.
  EXPECT_FALSE(matches_committed_origin("https://foo.com/", false));
  EXPECT_FALSE(matches_committed_origin("http://sub.foo.com/", false));
  EXPECT_FALSE(matches_committed_origin("http://foo.com:1234/", false));
  EXPECT_FALSE(matches_committed_origin("http://bar.com/", false));
  EXPECT_FALSE(matches_committed_origin("about:blank", false));
  EXPECT_FALSE(matches_committed_origin("data:,foo", false));
  EXPECT_FALSE(matches_committed_origin("file:///", false));

  // If the URL is for a precursor, ideally it should not match a non-opaque
  // committed origin, but this has not been implemented yet - see TODO in
  // ProcessState::MatchesCommittedOrigin().
  //
  // TODO(crbug.com/40148776): Flip this expectation to false after fixing the
  // dedicated workers case.
  EXPECT_TRUE(matches_committed_origin("http://foo.com/", true));

  // Add a second committed origin. Ensure both origins are matched properly.
  auto bar_origin = url::Origin::Create(GURL("http://bar.com"));
  p->AddCommittedOrigin(renderer_id, bar_origin);
  EXPECT_TRUE(matches_committed_origin("http://foo.com/", false));
  EXPECT_TRUE(matches_committed_origin("http://bar.com/", false));
  // TODO(crbug.com/40148776): Flip this expectation to false after fixing the
  // dedicated workers case.
  EXPECT_TRUE(matches_committed_origin("http://bar.com/", true));

  // Add a third committed origin, an opaque origin with the http://foo.com
  // precursor.
  auto opaque_with_foo_precursor = foo_origin.DeriveNewOpaqueOrigin();
  p->AddCommittedOrigin(renderer_id, opaque_with_foo_precursor);
  EXPECT_TRUE(matches_committed_origin("http://foo.com/", true));

  // These should still be true since both of these non-opaque origins have been
  // committed.
  EXPECT_TRUE(matches_committed_origin("http://foo.com/", false));
  EXPECT_TRUE(matches_committed_origin("http://bar.com/", false));

  // Add another committed origin for a file URL.
  p->AddCommittedOrigin(renderer_id,
                        url::Origin::Create(GURL("file:///etc/passwd")));
  EXPECT_TRUE(matches_committed_origin("file:///", false));
  // TODO(alexmos): This behavior of allowing file URL origins to match
  // regardless of their hosts might change in the future. See note in
  // ProcessState::MatchesCommittedOrigin().
  EXPECT_TRUE(matches_committed_origin("file://localhost/", false));
}

TEST_P(ChildProcessSecurityPolicyTest, SandboxedProcessEnforcements) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext browser_context;
  MockRenderProcessHost process(&browser_context);
  const int renderer_id = process.GetDeprecatedID();

  // Create a ProcessLock for a process-isolated sandboxed frame, and lock
  // `process` to it.
  UrlInfo sandboxed_url_info(
      UrlInfoInit(GURL("https://foo.com")).WithSandbox(true));
  scoped_refptr<SiteInstanceImpl> sandboxed_instance =
      SiteInstanceImpl::CreateForUrlInfo(&browser_context, sandboxed_url_info,
                                         /*is_guest=*/false,
                                         /*is_fenced=*/false,
                                         /*is_fixed_storage_partition=*/false);
  process.SetProcessLock(
      sandboxed_instance->GetIsolationContext(),
      ProcessLock::FromSiteInfo(sandboxed_instance->GetSiteInfo()));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  auto opaque_foo_origin = foo_origin.DeriveNewOpaqueOrigin();
  auto bar_origin = url::Origin::Create(GURL("https://bar.com"));
  auto opaque_bar_origin = bar_origin.DeriveNewOpaqueOrigin();

  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  // A sandboxed process should be able to commit new URLs, as long as they
  // have an opaque origin with a matching precursor.
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, opaque_foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  // TODO(crbug.com/325410297): Currently, non-opaque origins are allowed to
  // commit. Fix this and flip the expectation to false.
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, bar_origin,
                                  AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, opaque_bar_origin,
                                  AccessType::kCanCommitNewOrigin));

  // A sandboxed process should not be able to access data for any origin,
  // including origins it has committed.
  p->AddCommittedOrigin(renderer_id, opaque_foo_origin);
  EXPECT_FALSE(
      p->CanAccessOrigin(renderer_id, opaque_foo_origin,
                         AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      renderer_id, foo_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      renderer_id, bar_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(renderer_id, opaque_bar_origin,
                         AccessType::kCanAccessDataForCommittedOrigin));

  // A sandboxed process should only be able to claim that it has an opaque
  // origin.
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, opaque_foo_origin,
                                 AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(renderer_id, foo_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(renderer_id, bar_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, opaque_bar_origin,
                                  AccessType::kHostsOrigin));
}

// Verify that kCanCommitNewOrigin checks, which obtain the process's
// BrowserContext from its RenderProcessHost, fail if the RenderProcessHost is
// gone.
TEST_P(ChildProcessSecurityPolicyTest,
       CanCommitNewOriginRequiresRenderProcessHost) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  TestBrowserContext browser_context;
  // The process below is locked to https://foo.com if needed. With full site
  // isolation (e.g., on desktop), this gives it a site lock, so the
  // kCanCommitNewOrigin check that succeeds goes through the Jail check. With
  // partial site isolation (e.g., on Android), foo.com doesn't require a
  // dedicated process, so the process gets a lock that allows any site, and
  // that check goes through the Citadel check instead. The check that fails
  // doesn't depend on the lock, since the RenderProcessHost check happens
  // before the Jail and Citadel checks.
  const GURL foo_url("https://foo.com");
  const url::Origin foo_origin = url::Origin::Create(foo_url);

  // A process with a live RenderProcessHost can commit.
  auto process = std::make_unique<MockRenderProcessHost>(&browser_context);
  LockProcessIfNeeded(process->GetID(), &browser_context, foo_url);
  const int renderer_id = process->GetDeprecatedID();
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, foo_origin,
                                 AccessType::kCanCommitNewOrigin));

  // Without a RenderProcessHost, there's no way to obtain the process's
  // BrowserContext, so kCanCommitNewOrigin checks should fail. (To test this,
  // use a Handle to keep the process's ProcessState alive after its
  // RenderProcessHost is destroyed.)
  auto handle = p->CreateHandle(process->GetID());
  process.reset();
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, foo_origin,
                                  AccessType::kCanCommitNewOrigin));
}

// Verify that kCanCommitNewOrigin checks fail once the process's BrowserContext
// has started shutting down.
TEST_P(ChildProcessSecurityPolicyTest,
       CanCommitNewOriginFailsAfterBrowserContextShutdownStarted) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  TestBrowserContext browser_context;
  // As in CanCommitNewOriginRequiresRenderProcessHost, the process is locked
  // to https://foo.com if needed, which results in a site lock with full site
  // isolation and a lock that allows any site with partial site isolation.
  const GURL foo_url("https://foo.com");
  const url::Origin foo_origin = url::Origin::Create(foo_url);

  MockRenderProcessHost process(&browser_context);
  LockProcessIfNeeded(process.GetID(), &browser_context, foo_url);
  const int renderer_id = process.GetDeprecatedID();
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, foo_origin,
                                 AccessType::kCanCommitNewOrigin));

  // Once the BrowserContext starts shutting down, kCanCommitNewOrigin checks
  // should fail, since some of the state they depend on (e.g., isolated
  // origins) has already been cleared for that BrowserContext.
  browser_context.NotifyWillBeDestroyed();
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, foo_origin,
                                  AccessType::kCanCommitNewOrigin));
}

TEST_P(ChildProcessSecurityPolicyTest, PdfProcessEnforcements) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext browser_context;
  MockRenderProcessHost process(&browser_context);
  const int renderer_id = process.GetDeprecatedID();

  // Create a ProcessLock for a PDF renderer, and lock `process` to it.
  UrlInfo pdf_url_info(
      UrlInfoInit(GURL("https://foo.com"))
          .WithEmbedderIsolationInfo(EmbedderIsolationInfo::CreateForPdf()));
  scoped_refptr<SiteInstanceImpl> pdf_instance =
      SiteInstanceImpl::CreateForUrlInfo(&browser_context, pdf_url_info,
                                         /*is_guest=*/false,
                                         /*is_fenced=*/false,
                                         /*is_fixed_storage_partition=*/false);
  process.SetProcessLock(
      pdf_instance->GetIsolationContext(),
      ProcessLock::FromSiteInfo(pdf_instance->GetSiteInfo()));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  auto bar_origin = url::Origin::Create(GURL("https://bar.com"));

  using AccessType = ChildProcessSecurityPolicyImpl::AccessType;

  // A PDF process should be able to commit new URLs that match its ProcessLock.
  EXPECT_TRUE(p->CanAccessOrigin(renderer_id, foo_origin,
                                 AccessType::kCanCommitNewOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(renderer_id, bar_origin,
                                  AccessType::kCanCommitNewOrigin));

  // A PDF process should also be able to host an origin that matches its
  // ProcessLock; for example, PDF documents can still use postMessage so they
  // need to use this to validate the source origin.
  p->AddCommittedOrigin(renderer_id, foo_origin);
  EXPECT_TRUE(
      p->CanAccessOrigin(renderer_id, foo_origin, AccessType::kHostsOrigin));
  EXPECT_FALSE(
      p->CanAccessOrigin(renderer_id, bar_origin, AccessType::kHostsOrigin));

  // A PDF process should not be able to access data for any origin, including
  // an origin that it has committed.
  EXPECT_FALSE(p->CanAccessOrigin(
      renderer_id, foo_origin, AccessType::kCanAccessDataForCommittedOrigin));
  EXPECT_FALSE(p->CanAccessOrigin(
      renderer_id, bar_origin, AccessType::kCanAccessDataForCommittedOrigin));
}

// Verify that a PDF process is denied access to the sandboxed filesystem of
// the origin it has committed. This mirrors the data-access expectations in
// PdfProcessEnforcements above for the FileSystemURL-based entry points.
TEST_P(ChildProcessSecurityPolicyTest, PdfProcessSandboxedFileSystem) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  p->RegisterFileSystemPermissionPolicy(storage::kFileSystemTypeTemporary,
                                        storage::FILE_PERMISSION_SANDBOX);

  TestBrowserContext browser_context;
  MockRenderProcessHost process(&browser_context);
  const ChildProcessId process_id = process.GetID();

  UrlInfo pdf_url_info(
      UrlInfoInit(GURL("https://foo.com"))
          .WithEmbedderIsolationInfo(EmbedderIsolationInfo::CreateForPdf()));
  scoped_refptr<SiteInstanceImpl> pdf_instance =
      SiteInstanceImpl::CreateForUrlInfo(&browser_context, pdf_url_info,
                                         /*is_guest=*/false,
                                         /*is_fenced=*/false,
                                         /*is_fixed_storage_partition=*/false);
  process.SetProcessLock(
      pdf_instance->GetIsolationContext(),
      ProcessLock::FromSiteInfo(pdf_instance->GetSiteInfo()));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  p->AddCommittedOrigin(process.GetDeprecatedID(), foo_origin);

  base::FilePath file(TEST_PATH("/dir/testfile"));
  file = file.NormalizePathSeparators();
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      blink::StorageKey::CreateFirstParty(foo_origin),
      storage::kFileSystemTypeTemporary, file);

  // A PDF process should not be able to access data for any origin, including
  // an origin that it has committed, so all sandboxed filesystem operations
  // for that origin should be rejected.
  EXPECT_FALSE(p->CanReadFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanWriteFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCreateFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCreateReadWriteFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanCopyIntoFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanDeleteFileSystemFile(process_id, url));
  EXPECT_FALSE(p->CanMoveFileSystemFile(process_id, url, url));
  EXPECT_FALSE(p->CanCopyFileSystemFile(process_id, url, url));

  auto handle = p->CreateHandle(process_id);
  EXPECT_FALSE(handle.CanReadFileSystemFile(url));
  EXPECT_FALSE(handle.CanWriteFileSystemFile(url));
  EXPECT_FALSE(handle.CanCreateFileSystemFile(url));
  EXPECT_FALSE(handle.CanDeleteFileSystemFile(url));
  EXPECT_FALSE(handle.CanMoveFileSystemFile(url, url));
  EXPECT_FALSE(handle.CanCopyFileSystemFile(url, url));
}

// Test the granting of origin permissions, and their interactions with
// granting scheme permissions.
TEST_P(ChildProcessSecurityPolicyTest, OriginGranting) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  GURL url_foo1(GetWebUIURL("foo/resource1"));
  GURL url_foo2(GetWebUIURL("foo/resource2"));
  GURL url_bar(GetWebUIURL("bar/resource3"));

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const int renderer_id = process->GetDeprecatedID();
  LockProcessIfNeeded(process->GetID(), browser_context(), url_foo1);

  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  p->GrantRequestOrigin(renderer_id, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  p->GrantCommitOrigin(renderer_id, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Make sure this doesn't overwrite the earlier commit grants.
  p->GrantRequestOrigin(renderer_id, url::Origin::Create(url_foo1));

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Create a handle that extends the lifetime of the ProcessState beyond the
  // RenderProcessHost's lifetime.
  auto handle = p->CreateHandle(process->GetID());
  process.reset();

  // Queries should still succeed while the Handle exists, except for commit
  // checks, which need the RenderProcessHost to obtain the process's
  // BrowserContext.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Queries should no longer succeed after the Handle is invalidated.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo1));
  EXPECT_TRUE(p->CanRedirectToURL(url_foo2));
  EXPECT_TRUE(p->CanRedirectToURL(url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));
}

// Test the granting of scheme permissions.
TEST_P(ChildProcessSecurityPolicyTest, SchemeGranting) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  RegisterTestScheme("testscheme");
  RegisterTestScheme("otherscheme");

  GURL url_foo1("testscheme:resource1");
  GURL url_foo2("testscheme:resource2");
  GURL url_bar("otherscheme:resource3");

  // Queries and grants for an unregistered process should be safe no-ops.
  const ChildProcessId unregistered_process_id =
      ChildProcessHost::GenerateChildProcessUniqueId();
  EXPECT_FALSE(p->CanRequestURL(unregistered_process_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(unregistered_process_id, url_foo1));
  // TODO(crbug.com/379869738): Remove GetUnsafeValue().
  p->GrantRequestScheme(unregistered_process_id.GetUnsafeValue(), "testscheme");
  p->GrantCommitScheme(unregistered_process_id.GetUnsafeValue(), "testscheme");
  EXPECT_FALSE(p->CanRequestURL(unregistered_process_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(unregistered_process_id, url_foo1));

  auto process = std::make_unique<MockRenderProcessHost>(browser_context());
  const int renderer_id = process->GetDeprecatedID();
  LockProcessIfNeeded(process->GetID(), browser_context(), url_foo1);

  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  p->GrantRequestScheme(renderer_id, "testscheme");

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  p->GrantCommitScheme(renderer_id, "testscheme");

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Make sure this doesn't overwrite the earlier commit grants.
  p->GrantRequestScheme(renderer_id, "testscheme");

  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Create a handle that extends the lifetime of the ProcessState beyond the
  // RenderProcessHost's lifetime.
  auto handle = p->CreateHandle(process->GetID());
  process.reset();

  // Queries should still succeed while the Handle exists, except for commit
  // checks, which need the RenderProcessHost to obtain the process's
  // BrowserContext.
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_TRUE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Grants after Remove should be ignored even while a Handle exists.
  p->GrantRequestScheme(renderer_id, "otherscheme");
  p->GrantCommitScheme(renderer_id, "otherscheme");
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));

  // Queries should no longer succeed after the Handle is invalidated.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanRequestURL(renderer_id, url_bar));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo1));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_foo2));
  EXPECT_FALSE(p->CanCommitURL(renderer_id, url_bar));
}

// Verifies ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins method.
TEST_P(ChildProcessSecurityPolicyTest, AddFutureIsolatedOrigins) {
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
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Verify deduplication of the argument.
  p->AddFutureIsolatedOrigins({foo, bar, bar}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(foo, bar));

  // Verify that the old set is extended (not replaced).
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz));

  // Verify deduplication against the old set.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz));

  // Verify deduplication considers scheme and port differences.  Note that
  // origins that differ only in ports map to the same key.
  p->AddFutureIsolatedOrigins({baz, baz_http_8000, baz_https_8000},
                              IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz, baz_http));

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
    EXPECT_THAT(p->GetIsolatedOrigins(),
                testing::UnorderedElementsAre(foo, quxfoo, bar, baz, baz_http));
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
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

TEST_P(ChildProcessSecurityPolicyTest, IsolateAllSuborigins) {
  IsolatedOriginPattern qux("https://qux.com");
  IsolatedOriginPattern etld1_wild("https://[*.]foo.com");
  IsolatedOriginPattern etld2_wild("https://[*.]bar.foo.com");
  url::Origin etld1 = url::Origin::Create(GURL("https://foo.com"));
  url::Origin etld2 = url::Origin::Create(GURL("https://bar.foo.com"));

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  TestBrowserContext context;
  IsolationContext isolation_context(&context);

  // Check we can add a single wildcard origin.
  p->AddFutureIsolatedOrigins({etld1_wild}, IsolatedOriginSource::TEST);

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(etld1));

  // Verify it behaves as a wildcard origin: a subdomain resolves to its own
  // site URL.
  EXPECT_EQ(
      GURL("https://sub.foo.com"),
      SiteInfo::CreateForTesting(isolation_context, GURL("https://sub.foo.com"))
          .site_url());

  // Add a conventional origin and check they can live side by side.
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(etld1, qux.origin()));

  // Verify it behaves as a conventional origin: a subdomain resolves to the
  // eTLD+1 site URL (or its own origin if OriginKeyedProcessesByDefault is
  // enabled).
  EXPECT_EQ(
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(&context)
          ? GURL("https://sub.qux.com")
          : GURL("https://qux.com"),
      SiteInfo::CreateForTesting(isolation_context, GURL("https://sub.qux.com"))
          .site_url());

  // Check that a wildcard domain within another wildcard domain can be added.
  p->AddFutureIsolatedOrigins({etld2_wild}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(etld1, etld2, qux.origin()));

  // Verify nested wildcard behavior.
  EXPECT_EQ(GURL("https://sub.bar.foo.com"),
            SiteInfo::CreateForTesting(isolation_context,
                                       GURL("https://sub.bar.foo.com"))
                .site_url());

  // Check that removing a single wildcard domain, that contains another
  // wildcard domain, doesn't affect the isolating behavior of the original
  // wildcard domain.
  p->RemoveIsolatedOriginForTesting(etld1);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(etld2, qux.origin()));

  // The outer wildcard is removed, so sub.foo.com resolves to the eTLD+1 site
  // URL again (or its own origin if OriginKeyedProcessesByDefault is enabled).
  EXPECT_EQ(
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(&context)
          ? GURL("https://sub.foo.com")
          : GURL("https://foo.com"),
      SiteInfo::CreateForTesting(isolation_context, GURL("https://sub.foo.com"))
          .site_url());
  // But the inner wildcard is still present, so its subdomain resolves to its
  // own site URL.
  EXPECT_EQ(GURL("https://sub.bar.foo.com"),
            SiteInfo::CreateForTesting(isolation_context,
                                       GURL("https://sub.bar.foo.com"))
                .site_url());

  // Removing remaining domains.
  p->RemoveIsolatedOriginForTesting(qux.origin());
  p->RemoveIsolatedOriginForTesting(etld2);

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

// Verify that the isolation behavior for wildcard and non-wildcard origins,
// singly or in concert, behaves correctly via calls to GetSiteForURL().
TEST_P(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       WildcardAndNonWildcardOrigins) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // There should be no isolated origins before this test starts.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Also ensure that the web-safe schemes are in a proper initialized state.
  EXPECT_TRUE(p->IsWebSafeScheme(url::kHttpsScheme));
  EXPECT_TRUE(p->IsWebSafeScheme(url::kDataScheme));

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

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

TEST_P(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       WildcardAndNonWildcardEmbedded) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // There should be no isolated origins before this test starts.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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

  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

// Verifies that isolated origins only apply to future BrowsingInstances.
TEST_P(ChildProcessSecurityPolicyTest, DynamicIsolatedOrigins) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  url::Origin qux = url::Origin::Create(GURL("https://qux.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Save the next BrowsingInstance ID to be created.  Because unit tests run
  // in batches, this isn't guaranteed to always be 1, for example if a
  // previous test in the same batch had already created a SiteInstance and
  // BrowsingInstance.
  BrowsingInstanceId initial_id(SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate foo.com and bar.com.
  p->AddFutureIsolatedOrigins({foo, bar}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(foo, bar));

  // Isolating bar.com again should have no effect.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::UnorderedElementsAre(foo, bar));

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
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz));

  // Isolating bar.com again should not update the old BrowsingInstance ID.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz));

  // Create another BrowsingInstance.
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com/"));
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 1),
            bar_instance->GetIsolationContext().browsing_instance_id());
  EXPECT_EQ(BrowsingInstanceId::FromUnsafeValue(initial_id.value() + 2),
            SiteInstanceImpl::NextBrowsingInstanceId());

  // Isolate qux.com.
  p->AddFutureIsolatedOrigins({qux}, IsolatedOriginSource::TEST);
  EXPECT_THAT(p->GetIsolatedOrigins(),
              testing::UnorderedElementsAre(foo, bar, baz, qux));

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
TEST_P(ChildProcessSecurityPolicyTest, IsIsolatedOriginWithEmptyHost) {
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
TEST_P(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForSpecificBrowserContexts) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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
  EXPECT_EQ(&context1, foo_instance->GetIsolationContext().browser_context());

  // Isolating foo.com in |context1| is allowed and should add a new
  // IsolatedOriginEntry.  This wouldn't introduce any additional isolation,
  // since foo.com is already isolated globally, but the new entry is
  // important, e.g. for persisting profile-specific isolated origins across
  // restarts.
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST, &context1);
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context2, initial_id, foo));

  // Isolating bar.com in |context1| again should have no effect.
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(bar));
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context1);
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context1, initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context2, initial_id, bar));

  // Isolate bar.com for |context2|, which should add a new
  // IsolatedOriginEntry.  Verify that the isolation took effect for
  // |initial_id + 1| (the current BrowsingInstance ID cutoff) only.
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context2);
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(bar));
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
  EXPECT_EQ(3, p->GetIsolatedOriginEntryCountForTesting(bar));
  EXPECT_FALSE(IsIsolatedOrigin(&context3, initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(&context3, initial_id.value() + 1, bar));

  // An attempt to re-add bar.com for a new profile should create a new
  // IsolatedOriginEntry, though it wouldn't provide any additional isolation,
  // since bar.com is already isolated globally.
  TestBrowserContext context4;
  p->AddFutureIsolatedOrigins({bar}, IsolatedOriginSource::TEST, &context4);
  EXPECT_EQ(4, p->GetIsolatedOriginEntryCountForTesting(bar));

  p->RemoveIsolatedOriginForTesting(foo);
  p->RemoveIsolatedOriginForTesting(bar);
}

// This test ensures that isolated origins associated with a specific
// BrowserContext are removed when that BrowserContext is destroyed.
TEST_P(ChildProcessSecurityPolicyTest,
       IsolatedOriginsRemovedWhenBrowserContextDestroyed) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin sub_foo = url::Origin::Create(GURL("https://sub.foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, sub_foo));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, foo));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));

  // Isolate sub.foo.com and bar.com in |context2|.
  p->AddFutureIsolatedOrigins({sub_foo, bar}, IsolatedOriginSource::TEST,
                              context2.get());
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(sub_foo));
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(bar));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, bar));
  EXPECT_FALSE(IsIsolatedOrigin(context2.get(), initial_id, foo));

  // Isolate baz.com in both BrowserContexts.
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST,
                              context1.get());
  p->AddFutureIsolatedOrigins({baz}, IsolatedOriginSource::TEST,
                              context2.get());

  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(baz));
  EXPECT_TRUE(IsIsolatedOrigin(context1.get(), initial_id, baz));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, baz));

  // Remove |context1|.  foo.com should no longer be in the isolated_origins_
  // map, and the other origins should be isolated only in |context2|.
  context1.reset();

  EXPECT_EQ(0, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(sub_foo));
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(bar));
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(baz));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, sub_foo));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, bar));
  EXPECT_TRUE(IsIsolatedOrigin(context2.get(), initial_id, baz));

  // Remove |context2| and ensure the remaining entries are removed.
  context2.reset();
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());
}

TEST_P(ChildProcessSecurityPolicyTest, IsolatedOriginPattern) {
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
TEST_P(ChildProcessSecurityPolicyTest, GetIsolatedOrigins) {
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
TEST_P(ChildProcessSecurityPolicyTest, GetIsolatedOriginsWithProfile) {
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

TEST_P(ChildProcessSecurityPolicyTest, IsolatedOriginPatternEquality) {
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
TEST_P(ChildProcessSecurityPolicyTest, ParseIsolatedOrigins) {
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
TEST_P(ChildProcessSecurityPolicyTest, WildcardDefaultPort) {
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

// Verify the mechanism that allows non-origin-keyed isolated origins to be
// associated with a single BrowsingInstance.
TEST_P(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForSpecificBrowsingInstances) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  url::Origin baz = url::Origin::Create(GURL("https://baz.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

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
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));

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
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));

  // Isolate baz.com in `baz_browsing_instance`'s BrowsingInstance.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      baz_instance->GetIsolationContext(), baz, IsolatedOriginSource::TEST);
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(baz));

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
  EXPECT_EQ(3, p->GetIsolatedOriginEntryCountForTesting(foo));

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
TEST_P(ChildProcessSecurityPolicyTest,
       IsolatedOriginsForCurrentAndFutureBrowsingInstances) {
  url::Origin foo = url::Origin::Create(GURL("https://foo.com/"));
  url::Origin bar = url::Origin::Create(GURL("https://bar.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Initially there should be no isolated origins.
  EXPECT_THAT(p->GetIsolatedOrigins(), testing::IsEmpty());

  // Create a SiteInstance for foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com/"));
  auto foo_browsing_instance_id =
      foo_instance->GetIsolationContext().browsing_instance_id();

  // Isolate foo.com for `foo_instance`'s BrowsingInstance only.
  p->AddCoopIsolatedOriginForBrowsingInstance(
      foo_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(1, p->GetIsolatedOriginEntryCountForTesting(foo));

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
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));

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
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));

  // Likewise, an attempt to re-add foo.com for future BrowsingInstances should
  // be ignored.
  p->AddFutureIsolatedOrigins({foo}, IsolatedOriginSource::TEST, &context);
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));

  // However, we can still add foo.com isolation to a BrowsingInstance that
  // precedes `future_id` and doesn't match `foo_browsing_instance_id`.  Check
  // this with `bar_instance`'s BrowsingInstance.
  EXPECT_LT(bar_browsing_instance_id, future_id);
  p->AddCoopIsolatedOriginForBrowsingInstance(
      bar_instance->GetIsolationContext(), foo, IsolatedOriginSource::TEST);
  EXPECT_EQ(3, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  // When foo_instance and its BrowsingInstance goes away, the corresponding
  // entry just for that BrowsingInstance entry should be destroyed, but other
  // entries should remain.
  p->SetBrowsingInstanceCleanupDelayForTesting(0);
  foo_instance.reset();
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));
  EXPECT_FALSE(IsIsolatedOrigin(&context, foo_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, bar_browsing_instance_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id, foo));
  EXPECT_TRUE(IsIsolatedOrigin(&context, future_id.value() + 42, foo));

  // Destroying a BrowsingInstance with ID `future_id` shouldn't affect the
  // entry that applies to future BrowsingInstances.
  future_instance.reset();
  EXPECT_EQ(2, p->GetIsolatedOriginEntryCountForTesting(foo));
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
TEST_P(ChildProcessSecurityPolicyTest, NoBrowsingInstanceIDs_OriginKeyed) {
  url::Origin foo = url::Origin::Create(GURL("https://sub.foo.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

  // Create a SiteInstance for sub.foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  MockRenderProcessHost process(&context);
  const ChildProcessId process_id = process.GetID();
  const int renderer_id = process.GetDeprecatedID();
  {
    auto oac_header_request =
        OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
            /*had_oac_request=*/true,
            /*requires_origin_keyed_process=*/true);
    UrlInfo url_info(
        UrlInfoInit(foo.GetURL()).WithOACHeaderRequest(oac_header_request));
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForUrlInfo(
            &context, url_info,
            /*is_guest=*/false,
            /*is_fenced=*/false,
            /*is_fixed_storage_partition=*/false);

    p->LockProcess(foo_instance->GetIsolationContext(), process_id,
                   ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo()));
    p->AddCommittedOrigin(renderer_id, foo);

    EXPECT_TRUE(p->GetProcessLock(process_id).IsLockedToSite());
    EXPECT_TRUE(
        p->GetProcessLock(process_id).agent_cluster_key().IsOriginKeyed());
    EXPECT_EQ(foo.GetURL(), p->GetProcessLock(process_id).GetProcessLockURL());

    EXPECT_TRUE(ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo())
                    .agent_cluster_key()
                    .IsOriginKeyed());
    EXPECT_TRUE(
        p->DetermineOriginAgentClusterIsolation(
             foo_instance->GetIsolationContext(), foo,
             OriginAgentClusterIsolationState::CreateNonIsolatedByDefault())
            .requires_origin_keyed_process());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with `process` have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(process_id));

  // Because the ProcessLock is origin-keyed, we expect sub.foo.com to match but
  // not foo.com.
  EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id, foo));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      renderer_id, url::Origin::Create(GURL("https://foo.com/"))));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      renderer_id, url::Origin::Create(GURL("https://bar.com/"))));
}

// This test verifies that CanAccessDataForOrigin returns true for a process id
// even if all BrowsingInstanceIDs for that process have been deleted, so long
// as the request matches the process' lock. This test sets a site-keyed lock.
TEST_P(ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
       NoBrowsingInstanceIDs_SiteKeyed) {
  url::Origin sub_foo_origin =
      url::Origin::Create(GURL("https://sub.foo.com/"));
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

  // Create a SiteInstance for sub.foo.com in a new BrowsingInstance.
  TestBrowserContext context;
  MockRenderProcessHost process(&context);
  const ChildProcessId process_id = process.GetID();
  const int renderer_id = process.GetDeprecatedID();
  {
    // Isolate foo.com so we can't get a default SiteInstance. This will mean
    // that https://sub.foo.com will end up in a site-keyed SiteInstance, which
    // is what we need.
    p->AddFutureIsolatedOrigins({url::Origin::Create(GURL("https://foo.com"))},
                                IsolatedOriginSource::TEST, &context);

    UrlInfo url_info(UrlInfoInit(sub_foo_origin.GetURL()));
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForUrlInfo(
            &context, url_info,
            /*is_guest=*/false,
            /*is_fenced=*/false,
            /*is_fixed_storage_partition=*/false);
    p->LockProcess(foo_instance->GetIsolationContext(), process_id,
                   ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo()));
    p->AddCommittedOrigin(renderer_id, sub_foo_origin);

    EXPECT_TRUE(p->GetProcessLock(process_id).IsLockedToSite());
    // Note: This might become true in the future if we convert legacy isolated
    // origins to create origin-keyed AgentClusterKeys instead of site-keyed.
    EXPECT_FALSE(
        p->GetProcessLock(process_id).agent_cluster_key().IsOriginKeyed());
    EXPECT_EQ(SiteInfo::GetSiteForOrigin(sub_foo_origin),
              p->GetProcessLock(process_id).agent_cluster_key().GetSite());

    EXPECT_FALSE(ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo())
                     .agent_cluster_key()
                     .IsOriginKeyed());
    EXPECT_FALSE(
        p->DetermineOriginAgentClusterIsolation(
             foo_instance->GetIsolationContext(), sub_foo_origin,
             OriginAgentClusterIsolationState::CreateNonIsolatedByDefault())
            .requires_origin_keyed_process());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with `process` have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(process_id));

  // Because the ProcessLock is site-keyed, it should match foo.com and all
  // sub-origins. However, because we're in the enforcement mode based on a list
  // of committed origins, then only the specific origin we've committed
  // (sub.foo.com) will be allowed access. The other origin (foo.com) would need
  // to also be committed to get access.
  EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id, sub_foo_origin));
  url::Origin foo_origin(url::Origin::Create(GURL("https://foo.com/")));
  EXPECT_FALSE(p->CanAccessDataForOrigin(renderer_id, foo_origin));
  p->AddCommittedOrigin(renderer_id, foo_origin);
  EXPECT_TRUE(p->CanAccessDataForOrigin(renderer_id, foo_origin));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      renderer_id, url::Origin::Create(GURL("https://bar.com/"))));
}

// This test verifies that CanAccessDataForOrigin returns false for a process id
// when all BrowsingInstanceIDs for that process have been deleted, and the
// ProcessLock has is_locked_to_site() = false, regardless of the url requested.
TEST_P(ChildProcessSecurityPolicyTest, NoBrowsingInstanceIDs_UnlockedProcess) {
  GURL foo_url = GURL("https://foo.com/");
  url::Origin foo = url::Origin::Create(foo_url);

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->SetBrowsingInstanceCleanupDelayForTesting(0);

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

  TestBrowserContext context;
  MockRenderProcessHost process(&context);
  const ChildProcessId process_id = process.GetID();
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(process_id));
  {
    scoped_refptr<SiteInstanceImpl> foo_instance =
        SiteInstanceImpl::CreateForTesting(&context, foo_url);
    // Lock the process with an "allow_any_site" lock, using `foo_instance`'s
    // BrowsingInstance rather than a fixed BrowsingInstanceId. This is
    // important when this test is run with other tests, as then the
    // BrowsingInstanceId will not be 1 in general.
    p->LockProcess(foo_instance->GetIsolationContext(), process_id,
                   ProcessLock::CreateAllowAnySite(
                       StoragePartitionConfig::CreateDefault(&context),
                       WebExposedIsolationInfo::CreateNonIsolated(),
                       /*cross_origin_isolation_key=*/std::nullopt,
                       context.UniqueToken()));

    EXPECT_TRUE(foo_instance->HasSite());
    if (ShouldUseDefaultSiteInstanceGroup()) {
      EXPECT_EQ(foo_instance->group(),
                foo_instance->DefaultSiteInstanceGroupForBrowsingInstance());
      EXPECT_EQ(foo_instance->GetSecurityPrincipal().GetDeprecatedSiteURL(),
                foo_url);
    } else {
      EXPECT_TRUE(foo_instance->IsDefaultSiteInstance());
      EXPECT_EQ(foo_instance->GetSiteInfo(),
                SiteInfo::CreateForDefaultSiteInstance(
                    foo_instance->GetIsolationContext(),
                    StoragePartitionConfig::CreateDefault(&context),
                    WebExposedIsolationInfo::CreateNonIsolated(),
                    /*cross_origin_isolation_key=*/std::nullopt,
                    SiteShard::kDefault));
    }
    EXPECT_FALSE(foo_instance->RequiresDedicatedProcess());
  }
  // At this point foo_instance has gone away, and all BrowsingInstanceIDs
  // associated with `process` have been cleaned up.
  EXPECT_EQ(static_cast<size_t>(0),
            p->BrowsingInstanceIdCountForTesting(process_id));

  EXPECT_FALSE(p->GetProcessLock(process_id).IsLockedToSite());
  // Ensure that we don't allow the process to keep accessing data for foo after
  // all of the BrowsingInstances are gone, since that would require checking
  // whether foo itself requires a dedicated process.
  EXPECT_FALSE(p->CanAccessDataForOrigin(process.GetDeprecatedID(), foo));
}

// Regression test for https://crbug.com/1324407.
TEST_P(ChildProcessSecurityPolicyTest, CannotLockUsedProcessToSite) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context;
  MockRenderProcessHost process(&context);
  const ChildProcessId process_id = process.GetID();

  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com"));
  scoped_refptr<SiteInstanceImpl> bar_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://bar.com"));

  // Start by putting foo.com into an allows-any-site process.
  p->LockProcess(
      foo_instance->GetIsolationContext(), process_id,
      ProcessLock::CreateAllowAnySite(
          StoragePartitionConfig::CreateDefault(&context),
          WebExposedIsolationInfo::CreateNonIsolated(),
          /*cross_origin_isolation_key=*/std::nullopt, context.UniqueToken()));
  EXPECT_TRUE(p->GetProcessLock(process_id).AllowsAnySite());
  EXPECT_FALSE(p->GetProcessLock(process_id).IsLockedToSite());

  // If the process is then considered used (e.g., by loading content), it
  // should not be possible to lock it to another site.
  p->SetProcessIsUsed(process_id);
  EXPECT_FALSE(p->GetProcessLock(process_id).is_unused());

#if defined(OFFICIAL_BUILD)
  // In official builds, Rust is compiled with `-Cpanic=immediate-abort` even
  // when `DCHECK_IS_ON()` is true, so Rust `panic!` aborts immediately without
  // logging a message.
  [[maybe_unused]] constexpr char kExpectedRustDeathMessage[] = "";
#else
  // Ensure that the Rust implementation caused the CHECK failure when Rust is
  // enabled.
  [[maybe_unused]] constexpr char kExpectedRustDeathMessage[] =
      "process_lock\\.rs[\\s\\S]*Cannot lock an already used process to "
      ".*bar\\.com";
#endif
  EXPECT_CHECK_DEATH_WITH(
      {
        p->LockProcess(bar_instance->GetIsolationContext(), process_id,
                       ProcessLock::FromSiteInfo(bar_instance->GetSiteInfo()));
      },
      IsRustProcessStateEnabled()
          ? kExpectedRustDeathMessage
          : "Cannot lock an already used process to .*bar\\.com");
}

// Tests that queries for GetProcessLock work after RenderProcessHost removal
// until the corresponding Handles are gone. See https://crbug.com/470831168.
TEST_P(ChildProcessSecurityPolicyTest, GetProcessLockAfterProcessRemoval) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  TestBrowserContext context;

  scoped_refptr<SiteInstanceImpl> foo_instance =
      SiteInstanceImpl::CreateForTesting(&context, GURL("https://foo.com"));

  // Lock process to foo.com.
  auto process = std::make_unique<MockRenderProcessHost>(&context);
  const ChildProcessId process_id = process->GetID();
  p->LockProcess(foo_instance->GetIsolationContext(), process_id,
                 ProcessLock::FromSiteInfo(foo_instance->GetSiteInfo()));
  EXPECT_TRUE(p->GetProcessLock(process_id).IsLockedToSite());
  EXPECT_FALSE(p->GetProcessLock(process_id).AllowsAnySite());

  // Create a handle that extends the lifetime of the ProcessState beyond the
  // RenderProcessHost's lifetime.
  auto handle = p->CreateHandle(process_id);
  process.reset();

  // Queries should still succeed while the Handle exists.
  EXPECT_TRUE(p->GetProcessLock(process_id).IsLockedToSite());

  // Queries should no longer succeed after the Handle is invalidated.
  handle = ChildProcessSecurityPolicyImpl::Handle();
  EXPECT_FALSE(p->GetProcessLock(process_id).IsLockedToSite());
}

// Test that
// ChildProcessSecurityPolicyImpl::AddV8OptimizationDisabledStateForOriginIfNotCached()
// ignores opaque origins.
TEST_P(ChildProcessSecurityPolicyTest, AddV8OptimizationStateForOpaqueOrigin) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  BrowsingInstanceId browsing_instance_id =
      SiteInstanceImpl::NextBrowsingInstanceId();
  url::Origin opaque_origin;

  p->AddV8OptimizationDisabledStateForOriginIfNotCached(
      browsing_instance_id, opaque_origin,
      /*are_v8_optimizations_disabled=*/false);
  std::optional<bool> are_v8_optimizations_disabled_result =
      p->LookupAreV8OptimizationsDisabled(browsing_instance_id, opaque_origin);
  EXPECT_FALSE(are_v8_optimizations_disabled_result.has_value());
}

// Test the behavior of
// ChildProcessSecurityPolicyImpl::AddV8OptimizationDisabledStateForOriginIfNotCached()
// for non-opaque origins.
TEST_P(ChildProcessSecurityPolicyTest,
       AddV8OptimizationStateForNonOpaqueOrigin) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  BrowsingInstanceId browsing_instance_id =
      BrowsingInstanceId::FromUnsafeValue(1);
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  p->AddV8OptimizationDisabledStateForOriginIfNotCached(
      browsing_instance_id, origin, /*are_v8_optimizations_disabled=*/false);
  EXPECT_EQ(std::optional<bool>(false),
            p->LookupAreV8OptimizationsDisabled(browsing_instance_id, origin));

  EXPECT_FALSE(
      p->LookupAreV8OptimizationsDisabled(
           browsing_instance_id, url::Origin::Create(GURL("https://bar.com")))
          .has_value());
  EXPECT_FALSE(p->LookupAreV8OptimizationsDisabled(
                    browsing_instance_id,
                    url::Origin::Create(GURL("https://subdomain.foo.com")))
                   .has_value());

  EXPECT_FALSE(p->LookupAreV8OptimizationsDisabled(
                    BrowsingInstanceId::FromUnsafeValue(2), origin)
                   .has_value());
}

TEST_P(ChildProcessSecurityPolicyTest, AddV8OptimizationState_AlreadyCached) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  BrowsingInstanceId browsing_instance_id =
      BrowsingInstanceId::FromUnsafeValue(1);
  url::Origin origin = url::Origin::Create(GURL("https://foo.com"));

  p->AddV8OptimizationDisabledStateForOriginIfNotCached(
      browsing_instance_id, origin, /*are_v8_optimizations_disabled=*/false);
  EXPECT_EQ(std::optional<bool>(false),
            p->LookupAreV8OptimizationsDisabled(browsing_instance_id, origin));

  // Check that calling AddV8OptimizationDisabledStateForOriginIfNotCached() is
  // a no-op if the value is already cached.
  p->AddV8OptimizationDisabledStateForOriginIfNotCached(
      browsing_instance_id, origin, /*are_v8_optimizations_disabled=*/true);
  EXPECT_EQ(std::optional<bool>(false),
            p->LookupAreV8OptimizationsDisabled(browsing_instance_id, origin));
}

TEST_P(ChildProcessSecurityPolicyTest,
       AddOriginAgentClusterStateForBrowsingInstanceConsistency) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin foo_origin = url::Origin::Create(GURL("https://foo.origin.com/"));

  BrowsingInstanceId browsing_instance_id =
      BrowsingInstanceId::FromUnsafeValue(1);
  IsolationContext isolation_context(
      browsing_instance_id, browser_context(), /*is_guest=*/false,
      /*is_fenced=*/false,
      OriginAgentClusterIsolationState::CreateNonIsolatedByDefault());

  OriginAgentClusterIsolationState oac_opt_in =
      OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
          /*had_oac_request=*/true, /*requires_origin_keyed_process=*/true);

  // Register the initial state.
  p->AddOriginAgentClusterStateForBrowsingInstance(isolation_context,
                                                   foo_origin, oac_opt_in);

  // Attempt to override it with a different state.
  OriginAgentClusterIsolationState oac_opt_out =
      OriginAgentClusterIsolationState::CreateNonIsolatedByHeader();
  p->AddOriginAgentClusterStateForBrowsingInstance(isolation_context,
                                                   foo_origin, oac_opt_out);

  // Verify that the initial state is preserved.
  std::optional<OriginAgentClusterIsolationState> result =
      p->LookupOriginAgentClusterStateForTesting(browsing_instance_id,
                                                 foo_origin);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(oac_opt_in, result.value());

  p->RemoveOriginAgentClusterState(browsing_instance_id);
}

TEST_P(ChildProcessSecurityPolicyTest,
       RecordDefaultOriginAgentClusterOriginIfNewConsistency) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin foo_origin = url::Origin::Create(GURL("https://foo.origin.com/"));

  BrowsingInstanceId browsing_instance_id =
      BrowsingInstanceId::FromUnsafeValue(2);
  IsolationContext isolation_context(
      browsing_instance_id, browser_context(), /*is_guest=*/false,
      /*is_fenced=*/false,
      OriginAgentClusterIsolationState::CreateNonIsolatedByDefault());

  OriginAgentClusterIsolationState oac_default =
      OriginAgentClusterIsolationState::CreateNonIsolatedByDefault();

  // Register the initial state.
  p->RecordDefaultOriginAgentClusterOriginIfNew(
      isolation_context, foo_origin,
      /*is_global_walk_or_frame_removal=*/true);

  // Attempt to override it with a different state. Note:
  // RecordDefaultOriginAgentClusterOriginIfNew uses the default isolation
  // state from isolation_context. Check that if we call
  // AddOriginAgentClusterStateForBrowsingInstance (which populates the same
  // map), it won't override what was set by
  // RecordDefaultOriginAgentClusterOriginIfNew.
  OriginAgentClusterIsolationState oac_opt_in =
      OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
          /*had_oac_request=*/true, /*requires_origin_keyed_process=*/true);
  p->AddOriginAgentClusterStateForBrowsingInstance(isolation_context,
                                                   foo_origin, oac_opt_in);

  // Verify that the initial state (oac_default) is preserved.
  std::optional<OriginAgentClusterIsolationState> result =
      p->LookupOriginAgentClusterStateForTesting(browsing_instance_id,
                                                 foo_origin);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(oac_default, result.value());

  p->RemoveOriginAgentClusterState(browsing_instance_id);
}

TEST_P(ChildProcessSecurityPolicyTest,
       BrowserFileAccess_GrantAndRevokeSingleToken) {
  auto* p = ChildProcessSecurityPolicyImpl::GetInstance();
  base::FilePath file(TEST_PATH("/foo/bar"));
  base::UnguessableToken token = base::UnguessableToken::Create();

  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file));

  p->GrantFileForBrowserUpload(token, file);
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file));

  p->RevokeFileForBrowserUpload(token);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file));
}

TEST_P(ChildProcessSecurityPolicyTest,
       BrowserFileAccess_MultipleTokensSameFile) {
  auto* p = ChildProcessSecurityPolicyImpl::GetInstance();
  base::FilePath file(TEST_PATH("/foo/bar"));
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();

  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file));

  p->GrantFileForBrowserUpload(token1, file);
  p->GrantFileForBrowserUpload(token2, file);
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file));

  p->RevokeFileForBrowserUpload(token1);
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file));

  p->RevokeFileForBrowserUpload(token2);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file));
}

TEST_P(ChildProcessSecurityPolicyTest,
       BrowserFileAccess_MultipleTokensDifferentFiles) {
  auto* p = ChildProcessSecurityPolicyImpl::GetInstance();
  base::FilePath file1(TEST_PATH("/foo/bar1"));
  base::FilePath file2(TEST_PATH("/foo/bar2"));
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();

  p->GrantFileForBrowserUpload(token1, file1);
  p->GrantFileForBrowserUpload(token2, file2);

  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file1));
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file2));

  p->RevokeFileForBrowserUpload(token1);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file1));
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file2));

  p->RevokeFileForBrowserUpload(token2);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file1));
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file2));
}

TEST_P(ChildProcessSecurityPolicyTest,
       BrowserFileAccess_OneTokenMultipleFiles) {
  auto* p = ChildProcessSecurityPolicyImpl::GetInstance();
  base::FilePath file1(TEST_PATH("/foo/bar1"));
  base::FilePath file2(TEST_PATH("/foo/bar2"));
  base::UnguessableToken token = base::UnguessableToken::Create();

  p->GrantFileForBrowserUpload(token, file1);
  p->GrantFileForBrowserUpload(token, file2);

  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file1));
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file2));

  p->RevokeFileForBrowserUpload(token);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file1));
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file2));
}

TEST_P(ChildProcessSecurityPolicyTest,
       BrowserFileAccess_RevokeNonExistentToken) {
  auto* p = ChildProcessSecurityPolicyImpl::GetInstance();
  base::FilePath file(TEST_PATH("/foo/bar"));
  base::UnguessableToken valid_token = base::UnguessableToken::Create();
  base::UnguessableToken invalid_token = base::UnguessableToken::Create();

  p->GrantFileForBrowserUpload(valid_token, file);
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file));

  // Revoking a token that was never granted access shouldn't crash or modify
  // other grants.
  p->RevokeFileForBrowserUpload(invalid_token);
  EXPECT_TRUE(p->CanReadFileForBrowserUpload(file));

  p->RevokeFileForBrowserUpload(valid_token);
  EXPECT_FALSE(p->CanReadFileForBrowserUpload(file));
}

// Tests that processes are only allowed to commit and access data for content
// in their own shard (see BrowserContext::SetSitesForEmbedderShard()).
TEST_P(ChildProcessSecurityPolicyTest, Shards) {
  // TODO(crbug.com/576113096): Support shards in the Rust ProcessState.
  if (IsRustProcessStateEnabled()) {
    GTEST_SKIP() << "Shards are not supported by the Rust ProcessState yet.";
  }

  // Shards are currently only supported when site isolation is disabled and
  // default SiteInstanceGroups are not used, which is the case for Android
  // WebView.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kDefaultSiteInstanceGroups);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kDisableSiteIsolation);
  scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
      switches::kSitePerProcess);
  // Put the isolated.com site in the embedder shard. All other sites are in
  // the default shard.
  browser_context()->SetSitesForEmbedderShard({
      net::SchemefulSite(GURL("http://isolated.com")),
      net::SchemefulSite(GURL("https://isolated.com")),
  });
  ASSERT_FALSE(SiteIsolationPolicy::UseDedicatedProcessesForAllSites());

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();

  // Use MockRenderProcessHosts rather than made-up process IDs, so that there
  // is a real RenderProcessHost for each process. This matches production and
  // ensures the commit checks below run the full Citadel checks in
  // ChildProcessSecurityPolicyImpl::PerformJailAndCitadelChecks(), including
  // the shard check.
  MockRenderProcessHost isolated_process(browser_context());
  MockRenderProcessHost default_process(browser_context());
  MockRenderProcessHost unassigned_process(browser_context());
  MockRenderProcessHost locked_process(browser_context());
  const ChildProcessId kIsolatedProcess = isolated_process.GetID();
  const ChildProcessId kDefaultProcess = default_process.GetID();
  const ChildProcessId kUnassignedProcess = unassigned_process.GetID();
  const ChildProcessId kLockedProcess = locked_process.GetID();
  const int kIsolatedId = isolated_process.GetDeprecatedID();
  const int kDefaultId = default_process.GetDeprecatedID();
  const int kUnassignedId = unassigned_process.GetDeprecatedID();

  const GURL kIsolatedUrl("http://isolated.com/");
  const GURL kIsolatedSubdomainUrl("http://sub.isolated.com/");
  const GURL kIsolatedBlobUrl("blob:http://isolated.com/1234");
  const GURL kUnisolatedUrl("http://unisolated.com/");
  const url::Origin kIsolatedOrigin = url::Origin::Create(kIsolatedUrl);
  const url::Origin kUnisolatedOrigin = url::Origin::Create(kUnisolatedUrl);

  for (ChildProcessId process :
       {kIsolatedProcess, kDefaultProcess, kUnassignedProcess}) {
    // This gives the process an "allow any site" ProcessLock. Until the process
    // is used, the lock is in the default shard, but the process can still be
    // used for any shard.
    LockProcessIfNeeded(process, browser_context());
    ProcessLock lock = p->GetProcessLock(process);
    EXPECT_TRUE(lock.AllowsAnySite());
    EXPECT_TRUE(lock.is_unused());
    EXPECT_EQ(SiteShard::kDefault, lock.shard());

    // An unused process can commit URLs from any shard; see the Citadel
    // exception for unused processes in PerformJailAndCitadelChecks().
    EXPECT_TRUE(p->CanCommitURL(process.GetUnsafeValue(), kIsolatedUrl));
    EXPECT_TRUE(p->CanCommitURL(process.GetUnsafeValue(), kUnisolatedUrl));
  }

  // Processes are assigned to a shard on their ProcessLock when they are first
  // used. A process that is marked as used without specifying a shard is in
  // the default shard.
  p->SetProcessIsUsed(kIsolatedProcess, SiteShard::kEmbedderSites);
  p->SetProcessIsUsed(kDefaultProcess, SiteShard::kDefault);
  p->SetProcessIsUsed(kUnassignedProcess);
  for (ChildProcessId process :
       {kIsolatedProcess, kDefaultProcess, kUnassignedProcess}) {
    EXPECT_TRUE(p->GetProcessLock(process).AllowsAnySite());
    EXPECT_FALSE(p->GetProcessLock(process).is_unused());
  }
  EXPECT_EQ(SiteShard::kEmbedderSites,
            p->GetProcessLock(kIsolatedProcess).shard());
  EXPECT_EQ(SiteShard::kDefault, p->GetProcessLock(kDefaultProcess).shard());
  EXPECT_EQ(SiteShard::kDefault, p->GetProcessLock(kUnassignedProcess).shard());

  // ProcessLocks in different shards are not equal.
  EXPECT_NE(p->GetProcessLock(kIsolatedProcess),
            p->GetProcessLock(kDefaultProcess));
  EXPECT_EQ(p->GetProcessLock(kDefaultProcess),
            p->GetProcessLock(kUnassignedProcess));

  // Marking a used process as used again for the same shard, or without
  // specifying a shard, does not change its shard.
  p->SetProcessIsUsed(kIsolatedProcess, SiteShard::kEmbedderSites);
  p->SetProcessIsUsed(kIsolatedProcess);
  EXPECT_EQ(SiteShard::kEmbedderSites,
            p->GetProcessLock(kIsolatedProcess).shard());
  p->SetProcessIsUsed(kUnassignedProcess, SiteShard::kDefault);
  EXPECT_EQ(SiteShard::kDefault, p->GetProcessLock(kUnassignedProcess).shard());

  // Processes that are locked to a site are always in the default shard.
  p->LockProcessForTesting(
      IsolationContext(
          BrowsingInstanceId(1), browser_context(),
          /*is_guest=*/false, /*is_fenced=*/false,
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              browser_context())),
      kLockedProcess, kUnisolatedUrl);
  EXPECT_TRUE(p->GetProcessLock(kLockedProcess).IsLockedToSite());
  p->SetProcessIsUsed(kLockedProcess, SiteShard::kDefault);
  EXPECT_FALSE(p->GetProcessLock(kLockedProcess).is_unused());
  EXPECT_EQ(SiteShard::kDefault, p->GetProcessLock(kLockedProcess).shard());

  // A used process can commit URLs in its own shard only. kIsolatedProcess and
  // kDefaultProcess differ only in the shard of their ProcessLocks, and each
  // can commit URLs in its own shard, so the failures for the other shard can
  // only come from the shard check.
  EXPECT_TRUE(p->CanCommitURL(kIsolatedId, kIsolatedUrl));
  EXPECT_TRUE(p->CanCommitURL(kIsolatedId, kIsolatedSubdomainUrl));
  EXPECT_TRUE(p->CanCommitURL(kIsolatedId, kIsolatedBlobUrl));
  EXPECT_FALSE(p->CanCommitURL(kIsolatedId, kUnisolatedUrl));

  EXPECT_TRUE(p->CanCommitURL(kDefaultId, kUnisolatedUrl));
  EXPECT_FALSE(p->CanCommitURL(kDefaultId, kIsolatedUrl));
  EXPECT_FALSE(p->CanCommitURL(kDefaultId, kIsolatedSubdomainUrl));
  EXPECT_FALSE(p->CanCommitURL(kDefaultId, kIsolatedBlobUrl));

  // A process that was marked as used without specifying a shard is in the
  // default shard.
  EXPECT_TRUE(p->CanCommitURL(kUnassignedId, kUnisolatedUrl));
  EXPECT_FALSE(p->CanCommitURL(kUnassignedId, kIsolatedUrl));

  // Data access is also restricted to the process's shard, including for
  // opaque origins (e.g. for data: URLs) whose precursor is in that shard.
  p->AddCommittedOrigin(kIsolatedId, kIsolatedOrigin);
  p->AddCommittedOrigin(kIsolatedId, kIsolatedOrigin.DeriveNewOpaqueOrigin());
  p->AddCommittedOrigin(kDefaultId, kUnisolatedOrigin);
  p->AddCommittedOrigin(kDefaultId, kUnisolatedOrigin.DeriveNewOpaqueOrigin());
  EXPECT_TRUE(p->CanAccessDataForOrigin(kIsolatedId, kIsolatedOrigin));
  EXPECT_TRUE(p->CanAccessDataForOrigin(
      kIsolatedId, kIsolatedOrigin.DeriveNewOpaqueOrigin()));
  EXPECT_FALSE(p->CanAccessDataForOrigin(kIsolatedId, kUnisolatedOrigin));
  EXPECT_TRUE(p->CanAccessDataForOrigin(kDefaultId, kUnisolatedOrigin));
  EXPECT_FALSE(p->CanAccessDataForOrigin(kDefaultId, kIsolatedOrigin));
  EXPECT_FALSE(p->CanAccessDataForOrigin(
      kDefaultId, kIsolatedOrigin.DeriveNewOpaqueOrigin()));

  // Changing the shard of a used process, or assigning a non-default shard to
  // a process that is locked to a site, indicates a bug in process selection
  // and is fatal.
  [[maybe_unused]] constexpr char expected_death_message[] =
      "Attempting to change the shard";
  EXPECT_CHECK_DEATH_WITH(
      p->SetProcessIsUsed(kIsolatedProcess, SiteShard::kDefault),
      expected_death_message);
  EXPECT_CHECK_DEATH_WITH(
      p->SetProcessIsUsed(kDefaultProcess, SiteShard::kEmbedderSites),
      expected_death_message);
  EXPECT_CHECK_DEATH_WITH(
      p->SetProcessIsUsed(kUnassignedProcess, SiteShard::kEmbedderSites),
      expected_death_message);
  EXPECT_CHECK_DEATH_WITH(
      p->SetProcessIsUsed(kLockedProcess, SiteShard::kEmbedderSites),
      expected_death_message);

  // The MockRenderProcessHosts remove their processes from
  // ChildProcessSecurityPolicy when they are destroyed.
}

// This intentionally excludes {kCppOnly, kProcessState} since that behaves
// the same as {kCppOnly, kMain} and is redundant to test separately.
const CpspTestParam kCpspTestParams[] = {
    {RustPolicy::kCppOnly, CpspRustFeature::kMain},
    {RustPolicy::kRustOnly, CpspRustFeature::kMain},
    {RustPolicy::kRustOnly, CpspRustFeature::kProcessState},
    {RustPolicy::kRustAndCpp, CpspRustFeature::kMain},
    {RustPolicy::kRustAndCpp, CpspRustFeature::kProcessState},
};

INSTANTIATE_TEST_SUITE_P(,
                         ChildProcessSecurityPolicyTest,
                         ::testing::ValuesIn(kCpspTestParams),
                         &ChildProcessSecurityPolicyTest::DescribeParams);

INSTANTIATE_TEST_SUITE_P(
    ,
    ChildProcessSecurityPolicyTest_NoOriginKeyedProcessesByDefault,
    ::testing::ValuesIn(kCpspTestParams),
    &ChildProcessSecurityPolicyTest::DescribeParams);

}  // namespace content
