// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include <stdint.h>

#include <array>
#include <ranges>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
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
#include "content/browser/url_info.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "crypto/hash.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "net/base/features.h"
#include "net/http/http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

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
         net::features::kSplitCacheByNetworkIsolationKey,
         blink::features::kInlineScriptCache},
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
    for (auto renderer_id : added_renderers_) {
      p->Remove(renderer_id);
    }
  }

  void TearDown() override {
    // The GeneratedCodeCache should not be exercised at all for the queries
    // done in this test since they are expected to be served from
    // PersistentCache.
    histogram_tester.ExpectTotalCount("SiteIsolatedCodeCache.JS.Behaviour", 0);
  }

  static bool IsSitePerProcessOrStricter() { return GetParam(); }

  void SetupRendererWithLock(ChildProcessId process_id,
                             const UrlInfo& url_info) {
    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    p->AddForTesting(process_id, &browser_context_);

    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForUrlInfo(
            &browser_context_, url_info,
            /*is_guest=*/false,
            /*is_fenced=*/false,
            /*is_fixed_storage_partition=*/false);
    ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
        site_instance->GetIsolationContext(), process_id, false,
        ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()));

    added_renderers_.push_back(process_id);
  }

  void SetupRendererWithLock(ChildProcessId process_id, const GURL& url) {
    SetupRendererWithLock(process_id, UrlInfo::CreateForTesting(url));
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester;
  std::vector<ChildProcessId> added_renderers_;
  TestBrowserContext browser_context_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
};

// PersistentCache is not supported on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)

// Tests that back-to-back contexts operating in the same directory don't
// conflict with one another.
TEST_P(CodeCacheHostImplTest, PersistentCacheRecreationSequencing) {
  base::FilePath cache_path =
      temp_dir_.GetPath().AppendASCII("recreation_test");
  const std::string data_str = "some data";
  const GURL url("http://test.com/script.js");
  net::NetworkIsolationKey nik(net::SchemefulSite{url},
                               net::SchemefulSite{url});

  // Create a context and put some data into it.
  auto context_a = base::MakeRefCounted<GeneratedCodeCacheContext>();
  context_a->Initialize(cache_path, 0);
  GeneratedCodeCacheContext::RunOrPostTask(
      context_a, FROM_HERE, base::BindLambdaForTesting([=]() {
        auto host = CodeCacheHostImpl::Create(
            ChildProcessId(1), context_a, nik,
            blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
        host->DidGenerateCacheableMetadata(
            blink::mojom::CodeCacheType::kJavascript, url, base::Time::Now(),
            mojo_base::BigBuffer(base::as_byte_span(data_str)));
      }));

  // Shut down the context, which posts a task to destroy the cache
  // asynchronously.
  context_a->Shutdown();
  context_a.reset();

  // Create a new context using the same path and use it.
  auto context_b = base::MakeRefCounted<GeneratedCodeCacheContext>();
  context_b->Initialize(cache_path, 0);
  base::test::TestFuture<void> future;
  GeneratedCodeCacheContext::RunOrPostTask(
      context_b, FROM_HERE,
      base::BindLambdaForTesting([context_b, &url, &nik,
                                  quit = base::BindPostTaskToCurrentDefault(
                                      future.GetCallback())]() mutable {
        auto host = CodeCacheHostImpl::Create(
            ChildProcessId(2), context_b, nik,
            blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

        host->FetchCachedCode(blink::mojom::CodeCacheType::kJavascript, url,
                              base::BindOnce(
                                  [](base::OnceClosure quit, base::Time time,
                                     mojo_base::BigBuffer buf) {
                                    // The data may or may not have been cached;
                                    // but for sure the process didn't crash.
                                    if (IsSitePerProcessOrStricter()) {
                                      EXPECT_EQ(buf.size(), 0U);
                                    } else {
                                      EXPECT_NE(buf.size(), 0U);
                                    }
                                    std::move(quit).Run();
                                  },
                                  std::move(quit)));
      }));
  EXPECT_TRUE(future.Wait());

  context_b->Shutdown();
}

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
  const ChildProcessId process_id(12);
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

  const ChildProcessId locked_process_id(12);
  SetupRendererWithLock(locked_process_id, url);

  // The lack of a SetupRendererWithLock call for this process ID means
  // `GetSecondaryKeyForCodeCache` will return an empty GURL, which causes this
  // renderer to use the shared context key.
  const ChildProcessId unlocked_process_id(24);

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

    const ChildProcessId process_id(12);
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

    const ChildProcessId process_id(24);
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

TEST_P(CodeCacheHostImplTest, GetPendingBackend) {
  base::RunLoop run_loop;

  const GURL url("http://example.com/script.js");
  net::NetworkIsolationKey nik(net::SchemefulSite{url},
                               net::SchemefulSite{url});
  const ChildProcessId process_id(12);
  SetupRendererWithLock(process_id, url);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_.get(), FROM_HERE,
      base::BindLambdaForTesting([&]() {
        auto host = CodeCacheHostImpl::Create(
            process_id, generated_code_cache_context_, nik,
            blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

        for (auto cache_type : {blink::mojom::CodeCacheType::kJavascript,
                                blink::mojom::CodeCacheType::kWebAssembly}) {
          base::test::TestFuture<
              std::optional<persistent_cache::PendingBackend>>
              future;
          host->GetPendingBackend(cache_type, future.GetCallback());
          EXPECT_TRUE(future.Get().has_value());
        }
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(CodeCacheHostImplTest, SourceKeyedCacheWriteAndRead) {
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));
  const GURL url("http://example.com/script.js");

  const std::string script_source = "console.log(42)";
  const std::array<uint8_t, 32> source_hash =
      crypto::hash::Sha256(script_source);

  net::NetworkIsolationKey nik(net::SchemefulSite{url},
                               net::SchemefulSite{url});
  const ChildProcessId process_id(12);
  SetupRendererWithLock(process_id, url);

  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          host->DidGenerateSourceKeyedCacheableMetadata(
              std::vector(std::from_range, source_hash), data.Clone());

          host->FetchSourceKeyedCachedCodeForTesting(
              source_hash,
              base::BindOnce(
                  [](base::RepeatingClosure quit_closure,
                     std::string expected_data, mojo_base::BigBuffer data) {
                    EXPECT_EQ(
                        expected_data,
                        std::string(reinterpret_cast<const char*>(data.data()),
                                    data.size()));
                    quit_closure.Run();
                  },
                  quit_closure, data_str));
        }));
    runloop.Run();
  }

  // Isolation check: a different site shouldn't have access.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GURL other_url("http://other.example/script.js");
    net::NetworkIsolationKey other_nik(net::SchemefulSite{other_url},
                                       net::SchemefulSite{other_url});
    const ChildProcessId other_process_id(24);
    SetupRendererWithLock(other_process_id, other_url);

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              other_process_id, generated_code_cache_context_, other_nik,
              blink::StorageKey::CreateFirstParty(
                  url::Origin::Create(other_url)));

          host->FetchSourceKeyedCachedCodeForTesting(
              source_hash, base::BindOnce(
                               [](base::RepeatingClosure quit_closure,
                                  mojo_base::BigBuffer data) {
                                 EXPECT_EQ(data.size(), 0U);
                                 quit_closure.Run();
                               },
                               quit_closure));
        }));
    runloop.Run();
  }
}

TEST_P(CodeCacheHostImplTest,
       SourceKeyedCacheLockedAndUnlockedProcessesShareNoData) {
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));
  const std::string other_data_str = "some other data";
  const mojo_base::BigBuffer other_data(base::as_byte_span(other_data_str));

  const GURL url("http://example.com/script.js");
  const url::Origin origin = url::Origin::Create(url);
  const std::string script_source = "console.log(42)";
  const std::array<uint8_t, 32> source_hash =
      crypto::hash::Sha256(script_source);
  const std::vector<uint8_t> source_hash_vec(std::from_range, source_hash);

  net::NetworkIsolationKey nik(net::SchemefulSite{url},
                               net::SchemefulSite{url});

  const ChildProcessId locked_process_id(12);
  SetupRendererWithLock(locked_process_id, url);

  const ChildProcessId unlocked_process_id(24);

  // Locked process stores data.
  {
    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          auto host = CodeCacheHostImpl::Create(
              locked_process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(origin));

          host->DidGenerateSourceKeyedCacheableMetadata(source_hash_vec,
                                                        data.Clone());
        }));
  }

  // Unlocked process.
  {
    base::RunLoop runloop;

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          base::test::TestFuture<mojo_base::BigBuffer> fetch_future;

          auto host = CodeCacheHostImpl::Create(
              unlocked_process_id, generated_code_cache_context_, nik,
              blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));

          // Unlocked process cannot read cache from the locked process.
          host->FetchSourceKeyedCachedCodeForTesting(
              source_hash, fetch_future.GetCallback());
          EXPECT_EQ(fetch_future.Get().size(), 0U);
          fetch_future.Clear();

          // Unlocked process cannot store any data because the site has already
          // been locked to a process.
          host->DidGenerateSourceKeyedCacheableMetadata(source_hash_vec,
                                                        other_data.Clone());
          host->FetchSourceKeyedCachedCodeForTesting(
              source_hash, fetch_future.GetCallback());
          EXPECT_EQ(fetch_future.Get().size(), 0U);

          runloop.Quit();
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

          host->FetchSourceKeyedCachedCodeForTesting(
              source_hash,
              base::BindOnce(
                  [](base::RepeatingClosure quit_closure,
                     std::string expected_data, mojo_base::BigBuffer data) {
                    EXPECT_EQ(
                        expected_data,
                        std::string(reinterpret_cast<const char*>(data.data()),
                                    data.size()));
                    quit_closure.Run();
                  },
                  quit_closure, data_str));
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
  const ChildProcessId kOpenWebProcessId(12);
  const GURL open_web_site("https://best.web.site.com/");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  // State for a WebUI page that also loads the above resource.
  const ChildProcessId kWebUiProcessId(13);
  const GURL web_ui_site(
      base::StrCat({kChromeUIScheme, "://some.chrome.page"}));
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
  const GURL resource_url(
      base::StrCat({kChromeUIScheme, "://settings/settings.js"}));

  // State for a WebUI page that loads the above resource.
  const ChildProcessId kWebUiProcessId(12);
  const GURL web_ui_site(base::StrCat({kChromeUIScheme, "://settings"}));
  SetupRendererWithLock(kWebUiProcessId, web_ui_site);

  // State for a site on the open web that wants to modify the cached data for
  // the above resource.
  const ChildProcessId kOpenWebProcessId(13);
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

// Tests that a PDF page does not see a resource cached by an open web site
// on the same origin. Validates that process separation is properly maintained
// in the V8 cache.
TEST_P(CodeCacheHostImplTest, PdfObliviousToOpenWeb) {
  base::RunLoop run_loop;

  // The URL of a resource loaded by both a site on the open web and a PDF
  // page.
  const GURL resource_url("https://victim.example.com/script.js");

  // State for a site on the open web that loads the above resource.
  const ChildProcessId kOpenWebProcessId(12);
  const GURL open_web_site("https://victim.example.com/");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  // State for a PDF page that also loads the above resource on the same site.
  // Note that this requires setting is_pdf to true in the starting UrlInfo.
  const ChildProcessId kPdfProcessId(13);
  UrlInfo url_info(
      UrlInfoInit(open_web_site)
          .WithEmbedderIsolationInfo(EmbedderIsolationInfo::CreateForPdf()));
  SetupRendererWithLock(kPdfProcessId, url_info);

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

        // Create a PDF page's cache and make sure the resource is absent.
        // It should NOT receive the contents of the HTML's V8 cache!
        auto pdf_host = CodeCacheHostImpl::Create(
            kPdfProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        pdf_host->FetchCachedCode(
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

// Tests that an origin-restricted sandboxed iframe does not see a resource
// cached by an open web site on the same origin.
TEST_P(CodeCacheHostImplTest, SandboxedIframeObliviousToOpenWeb) {
  base::RunLoop run_loop;

  // The URL of a resource loaded by both a site on the open web and a
  // sandboxed iframe.
  const GURL resource_url("https://victim.example.com/script.js");

  // State for a site on the open web that loads the above resource.
  const ChildProcessId kOpenWebProcessId(12);
  const GURL open_web_site("https://victim.example.com/");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  // State for a sandboxed iframe that also loads the above resource on the
  // same site. Note that this requires setting is_sandboxed to true in the
  // starting UrlInfo.
  const ChildProcessId kSandboxedProcessId(13);
  UrlInfo url_info(UrlInfoInit(open_web_site).WithSandbox(true));
  SetupRendererWithLock(kSandboxedProcessId, url_info);

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

        // Create a sandboxed iframe's cache and make sure the resource is
        // absent. It should NOT receive the contents of the HTML's V8 cache!
        auto sandboxed_host = CodeCacheHostImpl::Create(
            kSandboxedProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        sandboxed_host->FetchCachedCode(
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

TEST_P(CodeCacheHostImplTest, SourceKeyedCacheWebUiObliviousToOpenWeb) {
  base::RunLoop run_loop;

  const std::string script_source = "console.log(42)";
  const std::array<uint8_t, 32> source_hash =
      crypto::hash::Sha256(script_source);

  static constexpr ChildProcessId kOpenWebProcessId(12);
  const GURL open_web_site("https://external.example/");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  static constexpr ChildProcessId kWebUiProcessId(13);
  const GURL web_ui_site(
      base::StrCat({kChromeUIScheme, "://some.chrome.page"}));
  SetupRendererWithLock(kWebUiProcessId, web_ui_site);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_.get(), FROM_HERE,
      base::BindLambdaForTesting([&]() {
        auto open_web_host = CodeCacheHostImpl::Create(
            kOpenWebProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        open_web_host->DidGenerateSourceKeyedCacheableMetadata(
            std::vector(std::from_range, source_hash),
            mojo_base::BigBuffer(base::byte_span_from_cstring("hi")));

        auto web_ui_host = CodeCacheHostImpl::Create(
            kWebUiProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{web_ui_site},
                                     net::SchemefulSite{web_ui_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(web_ui_site)));
        base::test::TestFuture<mojo_base::BigBuffer> fetch_future;
        web_ui_host->FetchSourceKeyedCachedCodeForTesting(
            source_hash, fetch_future.GetCallback());
        EXPECT_EQ(fetch_future.Get().size(), 0U);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_P(CodeCacheHostImplTest, OpenWebObliviousToSourceKeyedWebUi) {
  base::RunLoop run_loop;

  const std::string script_source = "console.log(42)";
  const std::array<uint8_t, 32> source_hash =
      crypto::hash::Sha256(script_source);

  static constexpr ChildProcessId kWebUiProcessId(12);
  const GURL web_ui_site(base::StrCat({kChromeUIScheme, "://settings"}));
  SetupRendererWithLock(kWebUiProcessId, web_ui_site);

  static constexpr ChildProcessId kOpenWebProcessId(13);
  const GURL open_web_site("https://external.example");
  SetupRendererWithLock(kOpenWebProcessId, open_web_site);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_.get(), FROM_HERE,
      base::BindLambdaForTesting([&]() {
        base::test::TestFuture<mojo_base::BigBuffer> fetch_future;

        auto web_ui_host = CodeCacheHostImpl::Create(
            kWebUiProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{web_ui_site},
                                     net::SchemefulSite{web_ui_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(web_ui_site)));
        web_ui_host->DidGenerateSourceKeyedCacheableMetadata(
            std::vector(std::from_range, source_hash),
            mojo_base::BigBuffer(base::byte_span_from_cstring("hi")));
        // WebUI sites cannot store any data.
        web_ui_host->FetchSourceKeyedCachedCodeForTesting(
            source_hash, fetch_future.GetCallback());
        EXPECT_EQ(fetch_future.Get().size(), 0U);
        fetch_future.Clear();

        auto open_web_host = CodeCacheHostImpl::Create(
            kOpenWebProcessId, generated_code_cache_context_,
            net::NetworkIsolationKey(net::SchemefulSite{open_web_site},
                                     net::SchemefulSite{open_web_site}),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(open_web_site)));
        open_web_host->FetchSourceKeyedCachedCodeForTesting(
            source_hash, fetch_future.GetCallback());
        EXPECT_EQ(fetch_future.Get().size(), 0U);
        run_loop.Quit();
      }));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         CodeCacheHostImplTest,
                         testing::Values(true, false));

#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace content
