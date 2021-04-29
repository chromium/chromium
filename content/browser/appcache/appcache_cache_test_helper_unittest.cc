// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_cache_test_helper.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/appcache_update_job.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {
namespace appcache_cache_test_helper_unittest {

class AppCacheCacheTestHelperTest;

// Values should match values used in appcache_update_job.cc.
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);

const char kManifest1Contents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n";

// There are a handful of http accessible resources that we need to conduct
// these tests. Instead of running a separate server to host these resources,
// we mock them up.
class MockHttpServer {
 public:
  static GURL GetMockUrl(const std::string& path) {
    return GURL("http://mockhost/" + path);
  }

  static void GetMockResponse(const std::string& path,
                              std::string* headers,
                              std::string* body) {
    const char not_found_headers[] =
        "HTTP/1.1 404 NOT FOUND\n"
        "\n";
    (*headers) = std::string(not_found_headers, base::size(not_found_headers));
    (*body) = "";
  }
};

inline bool operator==(const AppCacheNamespace& lhs,
                       const AppCacheNamespace& rhs) {
  return lhs.type == rhs.type && lhs.namespace_url == rhs.namespace_url &&
         lhs.target_url == rhs.target_url;
}

class MockFrontend : public blink::mojom::AppCacheFrontend {
 public:
  MockFrontend()
      : ignore_progress_events_(false),
        verify_progress_events_(false),
        last_progress_total_(-1),
        last_progress_complete_(-1),
        start_update_trigger_(
            blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT),
        update_(nullptr) {}

  void CacheSelected(blink::mojom::AppCacheInfoPtr info) override {}

  void EventRaised(blink::mojom::AppCacheEventID event_id) override {
    raised_events_.push_back(event_id);

    // Trigger additional updates if requested.
    if (event_id == start_update_trigger_ && update_) {
      for (AppCacheHost* host : update_hosts_) {
        update_->StartUpdate(
            host, (host ? host->pending_master_entry_url() : GURL()));
      }
      update_hosts_.clear();  // only trigger once
    }
  }

  void ErrorEventRaised(
      blink::mojom::AppCacheErrorDetailsPtr details) override {
    error_message_ = details->message;
    EventRaised(blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
  }

  void ProgressEventRaised(const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override {
    if (!ignore_progress_events_)
      EventRaised(blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);

    if (verify_progress_events_) {
      EXPECT_GE(num_total, num_complete);
      EXPECT_GE(num_complete, 0);

      if (last_progress_total_ == -1) {
        // Should start at zero.
        EXPECT_EQ(0, num_complete);
      } else {
        // Total should be stable and complete should bump up by one at a time.
        EXPECT_EQ(last_progress_total_, num_total);
        EXPECT_EQ(last_progress_complete_ + 1, num_complete);
      }

      // Url should be valid for all except the 'final' event.
      if (num_total == num_complete)
        EXPECT_TRUE(url.is_empty());
      else
        EXPECT_TRUE(url.is_valid());

      last_progress_total_ = num_total;
      last_progress_complete_ = num_complete;
    }
  }

  void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override {}

  void SetSubresourceFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory)
      override {}

  void AddExpectedEvent(blink::mojom::AppCacheEventID event_id) {
    DCHECK(!ignore_progress_events_ ||
           event_id != blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    expected_events_.push_back(event_id);
  }

  void SetIgnoreProgressEvents(bool ignore) {
    // Some tests involve joining new hosts to an already running update job
    // or intentionally failing. The timing and sequencing of the progress
    // events generated by an update job are dependent on the behavior of
    // an external HTTP server. For jobs that do not run fully till completion,
    // due to either joining late or early exit, we skip monitoring the
    // progress events to avoid flakiness.
    ignore_progress_events_ = ignore;
  }

  void SetVerifyProgressEvents(bool verify) {
    verify_progress_events_ = verify;
  }

  void TriggerAdditionalUpdates(blink::mojom::AppCacheEventID trigger_event,
                                AppCacheUpdateJob* update) {
    start_update_trigger_ = trigger_event;
    update_ = update;
  }

  void AdditionalUpdateHost(AppCacheHost* host) {
    update_hosts_.push_back(host);
  }

  using RaisedEvents = std::vector<blink::mojom::AppCacheEventID>;
  RaisedEvents raised_events_;
  std::string error_message_;

  // Set the expected events if verification needs to happen asynchronously.
  RaisedEvents expected_events_;
  std::string expected_error_message_;

  bool ignore_progress_events_;

  bool verify_progress_events_;
  int last_progress_total_;
  int last_progress_complete_;

  // Add ability for frontend to add master entries to an inprogress update.
  blink::mojom::AppCacheEventID start_update_trigger_;
  AppCacheUpdateJob* update_;
  std::vector<AppCacheHost*> update_hosts_;
};

class AppCacheCacheTestHelperTest : public testing::Test {
 public:
  AppCacheCacheTestHelperTest()
      : process_id_(123),
        weak_partition_factory_(static_cast<StoragePartitionImpl*>(
            browser_context_.GetDefaultStoragePartition())) {}

  void SetUp() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(process_id_,
                                                       &browser_context_);
  }

  void TearDown() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(process_id_);
  }
  // Use a separate IO thread to run a test. Thread will be destroyed
  // when it goes out of scope.
  template <class Method>
  void RunTestOnUIThread(Method method) {
    base::RunLoop run_loop;
    test_completed_cb_ = run_loop.QuitClosure();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(method, base::Unretained(this)));
    run_loop.Run();
  }

  void BasicStart() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);

    // Create a cache without a manifest entry.  The manifest entry will be
    // added later.
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), -1);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    AppCacheCacheTestHelper::CacheEntries cache_entries;

    // Add cache entry for manifest.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->headers = std::move(headers);
    AppCacheCacheTestHelper::AddCacheEntry(
        &cache_entries, group_->manifest_url(), AppCacheEntry::EXPLICIT,
        /*expect_if_modified_since=*/"Sat, 29 Oct 1994 19:43:31 GMT",
        /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
        std::move(response_info), kManifest1Contents);

    cache_helper_ = std::make_unique<AppCacheCacheTestHelper>(
        service_.get(), group_->manifest_url(), cache, std::move(cache_entries),
        base::BindOnce(&AppCacheCacheTestHelperTest::BasicWriteFinished,
                       base::Unretained(this)));
    cache_helper_->Write();
    // Continues async in |BasicWriteFinished|.
  }

  void BasicWriteFinished(int result) {
    cache_helper_->PrepareForRead(
        group_->newest_complete_cache(),
        base::BindOnce(&AppCacheCacheTestHelperTest::BasicReadFinished,
                       base::Unretained(this)));
    cache_helper_->Read();
    // Continues async in |BasicReadFinished|.
  }

  void BasicReadFinished() {
    EXPECT_EQ(cache_helper_->cache_entries().size(),
              cache_helper_->read_cache_entries().size());
    for (auto it = cache_helper_->cache_entries().begin();
         it != cache_helper_->cache_entries().end(); ++it) {
      auto read_it = cache_helper_->read_cache_entries().find(it->first);

      EXPECT_EQ(it->second->response_info->headers->raw_headers(),
                read_it->second->response_info->headers->raw_headers());

      EXPECT_EQ(it->second->body, read_it->second->body);
    }

    Finished();
  }

  void Finished() {
    // We unwind the stack prior to finishing up to let stack-based objects
    // get deleted.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheCacheTestHelperTest::FinishedUnwound,
                                  base::Unretained(this)));
  }

  void FinishedUnwound() {
    // Clean up everything that was created on the IO thread.
    cache_helper_.reset();
    protect_newest_cache_ = nullptr;
    group_ = nullptr;
    hosts_.clear();
    frontends_.clear();
    response_infos_.clear();
    service_.reset(nullptr);

    std::move(test_completed_cb_).Run();
  }

  void MakeService() {
    service_ = std::make_unique<MockAppCacheService>(
        weak_partition_factory_.GetWeakPtr());
  }

  AppCache* MakeCacheForGroup(int64_t cache_id, int64_t manifest_response_id) {
    return MakeCacheForGroup(cache_id, group_->manifest_url(),
                             manifest_response_id);
  }

  AppCache* MakeCacheForGroup(int64_t cache_id,
                              const GURL& manifest_entry_url,
                              int64_t manifest_response_id) {
    AppCache* cache = new AppCache(service_->storage(), cache_id);
    cache->set_complete(true);
    cache->set_manifest_parser_version(1);
    cache->set_manifest_scope("/");
    cache->set_update_time(base::Time::Now() - kOneHour);
    group_->AddCache(cache);
    group_->set_last_full_update_check_time(cache->update_time());

    // Add manifest entry to cache.
    if (manifest_response_id >= 0) {
      cache->AddEntry(manifest_entry_url, AppCacheEntry(AppCacheEntry::MANIFEST,
                                                        manifest_response_id));
    }

    // Specific tests that expect a newer time should set
    // expect_full_update_time_newer_than_ which causes this
    // equality expectation to be ignored.
    expect_full_update_time_equal_to_ = cache->update_time();

    return cache;
  }

  AppCacheHost* MakeHost(blink::mojom::AppCacheFrontend* frontend) {
    constexpr int kRenderFrameIdForTests = 456;
    hosts_.push_back(std::make_unique<AppCacheHost>(
        base::UnguessableToken::Create(), process_id_, kRenderFrameIdForTests,
        ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
            process_id_),
        mojo::NullRemote(), service_.get()));
    hosts_.back()->set_frontend_for_testing(frontend);
    return hosts_.back().get();
  }

  AppCacheResponseInfo* MakeAppCacheResponseInfo(
      const GURL& manifest_url,
      int64_t response_id,
      const std::string& raw_headers) {
    std::unique_ptr<net::HttpResponseInfo> http_info =
        std::make_unique<net::HttpResponseInfo>();
    http_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
    auto info = base::MakeRefCounted<AppCacheResponseInfo>(
        service_->storage()->GetWeakPtr(), manifest_url, response_id,
        std::move(http_info), 0);
    response_infos_.emplace_back(info);
    return info.get();
  }

  MockFrontend* MakeMockFrontend() {
    frontends_.push_back(std::make_unique<MockFrontend>());
    return frontends_.back().get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockAppCacheService> service_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> protect_newest_cache_;
  base::OnceClosure test_completed_cb_;

  std::unique_ptr<AppCacheResponseWriter> response_writer_;
  std::unique_ptr<AppCacheCacheTestHelper> cache_helper_;

  // Hosts used by an async test that need to live until update job finishes.
  // Otherwise, test can put host on the stack instead of here.
  std::vector<std::unique_ptr<AppCacheHost>> hosts_;

  // Response infos used by an async test that need to live until update job
  // finishes.
  std::vector<scoped_refptr<AppCacheResponseInfo>> response_infos_;

  // Flag indicating if test cares to verify the update after update finishes.
  base::Time expect_full_update_time_equal_to_;
  std::vector<std::unique_ptr<MockFrontend>>
      frontends_;  // to check expected events
  AppCache::EntryMap expect_extra_entries_;
  std::map<GURL, int64_t> expect_response_ids_;

  content::TestBrowserContext browser_context_;
  const int process_id_;
  base::WeakPtrFactory<StoragePartitionImpl> weak_partition_factory_;
};

TEST_F(AppCacheCacheTestHelperTest, Basic) {
  RunTestOnUIThread(&AppCacheCacheTestHelperTest::BasicStart);
}

}  // namespace appcache_cache_test_helper_unittest
}  // namespace content
