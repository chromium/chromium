// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "net/base/features.h"
#include "net/http/http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// Parametrized on whether strict isolation is activated or not.
class CodeCacheHostImplTest : public testing::Test,
                              public testing::WithParamInterface<bool> {
 public:
  CodeCacheHostImplTest() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (IsSitePerProcessOrStricter()) {
      command_line->AppendSwitch(switches::kSitePerProcess);
    } else {
      command_line->RemoveSwitch(switches::kSitePerProcess);
      command_line->AppendSwitch(switches::kDisableSiteIsolation);
    }

    feature_list_.InitWithFeatures(
        {blink::features::kUsePersistentCacheForCodeCache,
         net::features::kSplitCacheByNetworkIsolationKey},
        {});
    CHECK(temp_dir_.CreateUniqueTempDir());
    generated_code_cache_context_ =
        base::MakeRefCounted<GeneratedCodeCacheContext>();
    generated_code_cache_context_->Initialize(temp_dir_.GetPath(), 0);
  }

  ~CodeCacheHostImplTest() override {
    generated_code_cache_context_->Shutdown();

    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    for (int renderer_id : added_renderers_) {
      p->Remove(renderer_id);
    }
  }

  void TearDown() override {
    // The GeneratedCodeCache should not be exercised at all for the queries
    // done in this test since they are expected to be served from
    // PersistentCache.
    histogram_tester.ExpectTotalCount("SiteIsolatedCodeCache.JS.Behaviour", 0);
  }

  bool IsSitePerProcessOrStricter() { return GetParam(); }

  void SetupRendererWithLock(int process_id, const GURL& url) {
    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    p->AddForTesting(process_id, &browser_context_);

    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForTesting(&browser_context_, url);
    ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
        site_instance->GetIsolationContext(), process_id, false,
        ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()));

    added_renderers_.push_back(process_id);
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester;
  std::vector<int> added_renderers_;
  TestBrowserContext browser_context_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
};

// PersistentCache is not supported on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)

// Validates that improper site isolation setup (no valid process lock in
// renderer) leads to no use of the cache if full site isolation is activated.
// Under !IsSitePerProcessOrStricter() it instead means that the cache is usable
// and that entries are grouped under the same context for unlocked processes.
TEST_P(CodeCacheHostImplTest, PersistentCacheNoCachingWhenNoProperIsolation) {
  const base::Time response_time = base::Time::Now();
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));
  const GURL url("http://example.com/script.js");

  // The lack of a SetupRendererWithLock call for this process ID means
  // `GetSecondaryKeyForCodeCache` will return an empty GURL, which causes this
  // renderer to use the shared context key.
  const int process_id = 12;
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    net::NetworkIsolationKey nik(net::SchemefulSite{url},
                                 net::SchemefulSite{url});

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          // No way to confirm right away but storing should fail when
          // !IsSitePerProcessOrStricter().
          host->DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, url, response_time,
              data.Clone());

          if (IsSitePerProcessOrStricter()) {
            // No data was stored so fetching fails.
            host->FetchCachedCode(
                blink::mojom::CodeCacheType::kJavascript, url,
                base::BindOnce(
                    [&](base::OnceClosure quit_closure,
                        base::Time response_time, mojo_base::BigBuffer data) {
                      // Ensure data is empty.
                      EXPECT_EQ(response_time, base::Time());
                      EXPECT_EQ(data.byte_span(),
                                mojo_base::BigBuffer().byte_span());
                      std::move(quit_closure).Run();
                    },
                    quit_closure));
          } else {
            // Fetching of stored data succeeds.
            host->FetchCachedCode(
                blink::mojom::CodeCacheType::kJavascript, url,
                base::BindOnce(
                    [&](base::Time expected_response_time,
                        const std::string& expected_data,
                        base::OnceClosure quit_closure,
                        base::Time response_time, mojo_base::BigBuffer data) {
                      EXPECT_EQ(expected_response_time, response_time);
                      EXPECT_EQ(expected_data,
                                std::string(
                                    reinterpret_cast<const char*>(data.data()),
                                    data.size()));
                      std::move(quit_closure).Run();
                    },
                    response_time, data_str, quit_closure));
          }
        }));
    runloop.Run();
  }
}

TEST_P(CodeCacheHostImplTest,
       PersistentCacheLockedAndUnlockedProcessesShareNoData) {
  const base::Time response_time = base::Time::Now();
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));

  const base::Time other_response_time = response_time + base::Minutes(1);
  const std::string other_data_str = "some other data";
  const mojo_base::BigBuffer other_data(base::as_byte_span(other_data_str));

  const GURL url("http://example.com/script.js");
  net::NetworkIsolationKey nik(net::SchemefulSite{url},
                               net::SchemefulSite{url});

  const int locked_process_id = 12;
  SetupRendererWithLock(locked_process_id, url);

  // The lack of a SetupRendererWithLock call for this process ID means
  // `GetSecondaryKeyForCodeCache` will return an empty GURL, which causes this
  // renderer to use the shared context key.
  const int unlocked_process_id = 24;

  // Locked process stores data.
  {
    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              locked_process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          host->DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, url, response_time,
              data.Clone());
        }));
  }

  // Unlocked process.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              unlocked_process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          // Unlocked process cannot see the data stored by the locked process,
          // even with the same NIK.
          host->FetchCachedCode(
              blink::mojom::CodeCacheType::kJavascript, url,

              base::BindOnce(
                  [&](base::OnceClosure quit_closure, base::Time response_time,
                      mojo_base::BigBuffer data) {
                    // Ensure data is empty.
                    EXPECT_EQ(response_time, base::Time());
                    EXPECT_EQ(data.byte_span(),
                              mojo_base::BigBuffer().byte_span());
                    std::move(quit_closure).Run();
                  },
                  quit_closure));

          // Store some other data to attempt to retrieve it from the locked
          // process.
          host->DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, url,
              other_response_time, other_data.Clone());
        }));
    runloop.Run();
  }

  // Locked process cannot access data stored from shared context.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              locked_process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          // Locked process cannot see the data stored by the unlocked process
          // and sees its own copy, even with the same NIK.
          host->FetchCachedCode(
              blink::mojom::CodeCacheType::kJavascript, url,

              base::BindOnce(
                  [&](base::Time expected_response_time,
                      const std::string& expected_data,
                      base::OnceClosure quit_closure, base::Time response_time,
                      mojo_base::BigBuffer data) {
                    EXPECT_EQ(expected_response_time, response_time);
                    EXPECT_EQ(
                        expected_data,
                        std::string(reinterpret_cast<const char*>(data.data()),
                                    data.size()));
                    std::move(quit_closure).Run();
                  },
                  response_time, data_str, quit_closure));
        }));
    runloop.Run();
  }
}

// This test is parameterized to run with and without strict site isolation
// enabled. With a locked process, caching should succeed regardless of the
// isolation mode.
TEST_P(CodeCacheHostImplTest, PersistentCacheWriteAndReadFullIsolationSetup) {
  const base::Time response_time = base::Time::Now();
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));
  const GURL original_resource_url("http://example.com/script.js");

  // Storing and retrieving for from the same isolation context works.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GURL url = original_resource_url;
    net::NetworkIsolationKey nik(net::SchemefulSite{url},
                                 net::SchemefulSite{url});

    const int process_id = 12;
    SetupRendererWithLock(process_id, url);

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          host->DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, url, response_time,
              data.Clone());

          host->FetchCachedCode(
              blink::mojom::CodeCacheType::kJavascript, url,

              base::BindOnce(
                  [&](base::Time expected_response_time,
                      const std::string& expected_data,
                      base::OnceClosure quit_closure, base::Time response_time,
                      mojo_base::BigBuffer data) {
                    EXPECT_EQ(expected_response_time, response_time);
                    EXPECT_EQ(
                        expected_data,
                        std::string(reinterpret_cast<const char*>(data.data()),
                                    data.size()));
                    std::move(quit_closure).Run();
                  },
                  response_time, data_str, quit_closure));
        }));
    runloop.Run();
  }

  // Attempting to retrieve code for `original_resource_url` from a different
  // isolation context does not work and instead returns default values.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GURL url("http://other.com/script.js");
    net::NetworkIsolationKey nik(net::SchemefulSite{url},
                                 net::SchemefulSite{url});

    const int process_id = 24;
    SetupRendererWithLock(process_id, url);

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
          host->FetchCachedCode(
              blink::mojom::CodeCacheType::kJavascript, original_resource_url,

              base::BindOnce(
                  [&](base::OnceClosure quit_closure, base::Time response_time,
                      mojo_base::BigBuffer data) {
                    // Ensure data is empty.
                    EXPECT_EQ(response_time, base::Time());
                    EXPECT_EQ(data.byte_span(),
                              mojo_base::BigBuffer().byte_span());
                    std::move(quit_closure).Run();
                  },
                  quit_closure));
        }));
    runloop.Run();
  }
}

// Tests that a WebUI page does not see a resource cached by an open web site.
TEST_P(CodeCacheHostImplTest, WebUiObliviousToOpenWeb) {
  base::RunLoop run_loop;

  // The URL of a resource loaded by both a site on the one web and a WebUI
  // page.
  const GURL resource_url("https://best.web.site.com/script.js");

  // State for a site on the open web that loads the above resource.
  static constexpr int kOpenWebProcessId = 12;
  const GURL open_web_site("https://best.web.site.com/");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  // State for a WebUI page that also loads the above resource.
  static constexpr int kWebUiProcessId = 13;
  const GURL web_ui_site("chrome://some.chrome.page");
  SetupRendererWithLock(kWebUiProcessId, web_ui_site);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_.get(), FROM_HERE,
      base::BindLambdaForTesting([&]() {
        // Create the open web's cache and put the resource into it.
        auto open_web_host = CodeCacheHostImpl::Create(
            kOpenWebProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        open_web_host->DidGenerateCacheableMetadata(
            blink::mojom::CodeCacheType::kJavascript, resource_url,
            base::Time::Now(),
            mojo_base::BigBuffer(base::byte_span_from_cstring("hi")));

        // Create a WebUI page's cache and make sure the resource is absent.
        auto web_ui_host = CodeCacheHostImpl::Create(
            kWebUiProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{web_ui_site},
                                     net::SchemefulSite{web_ui_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(web_ui_site)));
        web_ui_host->FetchCachedCode(
            blink::mojom::CodeCacheType::kJavascript, resource_url,
            base::BindLambdaForTesting([&](base::Time found_response_time,
                                           mojo_base::BigBuffer found_data) {
              EXPECT_EQ(found_response_time, base::Time());
              EXPECT_EQ(found_data.size(), 0U);
              run_loop.Quit();
            }));
      }));

  run_loop.Run();
}

// Tests that a page on the open web can't access resources cached for a WebUI
// page.
TEST_P(CodeCacheHostImplTest, OpenWebObliviousToWebUi) {
  base::RunLoop run_loop;

  // The URL of a resource loaded by a WebUI page.
  const GURL resource_url("chrome://settings/settings.js");

  // State for a WebUI page that loads the above resource.
  static constexpr int kWebUiProcessId = 12;
  const GURL web_ui_site("chrome://settings");
  SetupRendererWithLock(kWebUiProcessId, web_ui_site);

  // State for a site on the open web that wants to modify the cached data for
  // the above resource.
  static constexpr int kOpenWebProcessId = 13;
  const GURL open_web_site("https://somewhere.com");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_.get(), FROM_HERE,
      base::BindLambdaForTesting([&]() {
        // Create the WebUI page's cache and put the resource into it.
        auto web_ui_host = CodeCacheHostImpl::Create(
            kWebUiProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{web_ui_site},
                                     net::SchemefulSite{web_ui_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(web_ui_site)));
        const base::Time web_ui_resource_time = base::Time::Now();
        const auto resource_data = base::byte_span_from_cstring("hi");
        web_ui_host->DidGenerateCacheableMetadata(
            blink::mojom::CodeCacheType::kJavascript, resource_url,
            web_ui_resource_time, mojo_base::BigBuffer(resource_data));

        // Create the open web site's cache and make sure the resource is
        // absent.
        auto open_web_host = CodeCacheHostImpl::Create(
            kOpenWebProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        base::test::TestFuture<base::Time, mojo_base::BigBuffer> fetch_future;
        open_web_host->FetchCachedCode(blink::mojom::CodeCacheType::kJavascript,
                                       resource_url,
                                       fetch_future.GetCallback());
        EXPECT_EQ(fetch_future.Get<0>(), base::Time());
        EXPECT_EQ(fetch_future.Get<1>().size(), 0U);
        fetch_future.Clear();

        {
          mojo::FakeMessageDispatchContext dispatch_context;
          // Insert into the open web site's cache in an attempt to corrupt the
          // cached data for the WebUI page.
          open_web_host->DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, resource_url,
              base::Time::Now(),
              mojo_base::BigBuffer(base::byte_span_from_cstring("nyuknyuk")));
        }
        // Verify that the WebUI page sees its own cached data if WebUI caching
        // is enabled or nothing otherwise.
        web_ui_host->FetchCachedCode(blink::mojom::CodeCacheType::kJavascript,
                                     resource_url, fetch_future.GetCallback());
        if (base::FeatureList::IsEnabled(features::kWebUICodeCache)) {
          EXPECT_EQ(fetch_future.Get<0>(), web_ui_resource_time);
          EXPECT_EQ(base::span(fetch_future.Get<1>()), resource_data);
        } else {
          EXPECT_EQ(fetch_future.Get<0>(), base::Time());
          EXPECT_EQ(base::span(fetch_future.Get<1>()).size(), 0U);
        }
        run_loop.Quit();
      }));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         CodeCacheHostImplTest,
                         testing::Values(true, false));

#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace content
