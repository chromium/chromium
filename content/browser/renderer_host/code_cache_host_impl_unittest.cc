// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/code_cache_host_impl.h"

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/features.h"
#include "net/http/http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class CodeCacheHostImplTest : public testing::Test {
 public:
  CodeCacheHostImplTest() {
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
  }

  void SetupRendererWithLock(int process_id, const GURL& url) {
    ChildProcessSecurityPolicyImpl* p =
        ChildProcessSecurityPolicyImpl::GetInstance();
    p->AddForTesting(process_id, &browser_context_);

    scoped_refptr<SiteInstanceImpl> site_instance =
        SiteInstanceImpl::CreateForTesting(&browser_context_, url);
    ChildProcessSecurityPolicyImpl::GetInstance()->LockProcess(
        site_instance->GetIsolationContext(), process_id, false,
        ProcessLock::FromSiteInfo(site_instance->GetSiteInfo()));
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;
};

#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(CodeCacheHostImplTest, PersistentCacheWriteAndRead) {
#else
// PersistentCache is not supported on Fuchsia.
TEST_F(CodeCacheHostImplTest, DISABLED_PersistentCacheWriteAndRead) {
#endif
  const base::Time response_time = base::Time::Now();
  const std::string data_str = "some data";
  const mojo_base::BigBuffer data(base::as_byte_span(data_str));
  const GURL original_resource_url("http://example.com/script.js");

  // Storing and retrieving for from the same isolation context works.
  {
    base::RunLoop runloop;
    auto quit_closure = runloop.QuitClosure();

    GURL url = original_resource_url;
    GURL origin_lock("http://example.com");
    net::NetworkIsolationKey nik(net::SchemefulSite{url},
                                 net::SchemefulSite{url});

    const int process_id = 12;
    SetupRendererWithLock(process_id, url);

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          CodeCacheHostImpl host(process_id, generated_code_cache_context_, nik,
                                 blink::StorageKey::CreateFirstParty(
                                     url::Origin::Create(origin_lock)));

          host.DidGenerateCacheableMetadata(
              blink::mojom::CodeCacheType::kJavascript, url, response_time,
              data.Clone());

          host.FetchCachedCode(
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
    GURL origin_lock("http://other.com");
    net::NetworkIsolationKey nik(net::SchemefulSite{url},
                                 net::SchemefulSite{url});

    const int process_id = 24;
    SetupRendererWithLock(process_id, url);

    GeneratedCodeCacheContext::RunOrPostTask(
        generated_code_cache_context_.get(), FROM_HERE,
        base::BindLambdaForTesting([&]() {
          CodeCacheHostImpl host(process_id, generated_code_cache_context_, nik,
                                 blink::StorageKey::CreateFirstParty(
                                     url::Origin::Create(origin_lock)));
          host.FetchCachedCode(
              blink::mojom::CodeCacheType::kJavascript, original_resource_url,

              base::BindOnce(
                  [&](base::OnceClosure quit_closure, base::Time response_time,
                      mojo_base::BigBuffer data) {
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

}  // namespace content
