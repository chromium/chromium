// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/mock_appcache_storage.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

namespace content {
namespace {

const int64_t kMockGroupId = 1;
const int64_t kMockCacheId = 1;
const int64_t kMockResponseId = 1;
const int64_t kMissingCacheId = 5;
const int64_t kMissingResponseId = 5;
const char kMockHeaders[] =
    "HTTP/1.0 200 OK\0Content-Length: 5\0\0";
const char kMockBody[] = "Hello";
const int kMockBodySize = 5;

class MockResponseReader : public AppCacheResponseReader {
 public:
  MockResponseReader(int64_t response_id,
                     std::unique_ptr<net::HttpResponseInfo> info,
                     int info_size,
                     const char* data,
                     int data_size)
      : AppCacheResponseReader(response_id, /*disk_cache=*/nullptr),
        info_(std::move(info)),
        info_size_(info_size),
        data_(data),
        data_size_(data_size) {}
  void ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                OnceCompletionCallback callback) override {
    info_buffer_ = info_buf;
    callback_ = std::move(callback);  // Cleared on completion.

    int rv = info_.get() ? info_size_ : net::ERR_FAILED;
    info_buffer_->http_info.reset(info_.release());
    info_buffer_->response_data_size = data_size_;
    ScheduleUserCallback(rv);
  }
  void ReadData(net::IOBuffer* buf,
                int buf_len,
                OnceCompletionCallback callback) override {
    buffer_ = buf;
    buffer_len_ = buf_len;
    callback_ = std::move(callback);  // Cleared on completion.

    if (!data_) {
      ScheduleUserCallback(net::ERR_CACHE_READ_FAILURE);
      return;
    }
    DCHECK(buf_len >= data_size_);
    memcpy(buf->data(), data_, data_size_);
    ScheduleUserCallback(data_size_);
    data_size_ = 0;
  }

 private:
  void ScheduleUserCallback(int result) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockResponseReader::InvokeUserCompletionCallback,
                       weak_factory_.GetWeakPtr(), result));
  }

  std::unique_ptr<net::HttpResponseInfo> info_;
  int info_size_;
  const char* data_;
  int data_size_;
};

}  // namespace


class AppCacheServiceImplTest : public testing::Test {
 public:
  AppCacheServiceImplTest()
      : kOriginURL("http://hello/"),
        kOrigin(url::Origin::Create(kOriginURL)),
        kManifestUrl(kOriginURL.Resolve("manifest")),
        service_(std::make_unique<AppCacheServiceImpl>(nullptr, nullptr)),
        delete_result_(net::OK),
        delete_completion_count_(0) {
    // Setup to use mock storage.
    service_->storage_ = std::make_unique<MockAppCacheStorage>(service_.get());
  }

  void OnDeleteAppCachesComplete(int result) {
    delete_result_ = result;
    ++delete_completion_count_;
  }

  MockAppCacheStorage* mock_storage() {
    return static_cast<MockAppCacheStorage*>(service_->storage());
  }

  void ResetStorage() {
    service_->storage_ = std::make_unique<MockAppCacheStorage>(service_.get());
  }

  bool IsGroupStored(const GURL& manifest_url) {
    return mock_storage()->IsGroupForManifestStored(manifest_url);
  }

  int CountPendingHelpers() {
    return service_->pending_helpers_.size();
  }

  void SetupMockGroup() {
    std::unique_ptr<net::HttpResponseInfo> info(MakeMockResponseInfo());
    const int kMockInfoSize = GetResponseInfoSize(info.get());

    // Create a mock group, cache, and entry and stuff them into mock storage.
    auto group = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), kManifestUrl, kMockGroupId);
    auto cache =
        base::MakeRefCounted<AppCache>(service_->storage(), kMockCacheId);
    cache->AddEntry(
        kManifestUrl,
        AppCacheEntry(AppCacheEntry::MANIFEST, kMockResponseId,
                      kMockInfoSize + kMockBodySize, /*padding_size=*/0));
    cache->set_complete(true);
    group->AddCache(cache.get());
    mock_storage()->AddStoredGroup(group.get());
    mock_storage()->AddStoredCache(cache.get());
  }

  void SetupMockReader(
      bool valid_info, bool valid_data, bool valid_size) {
    std::unique_ptr<net::HttpResponseInfo> info =
        valid_info ? MakeMockResponseInfo() : nullptr;
    int info_size = info ? GetResponseInfoSize(info.get()) : 0;
    const char* data = valid_data ? kMockBody : nullptr;
    int data_size = valid_size ? kMockBodySize : 3;
    mock_storage()->SimulateResponseReader(std::make_unique<MockResponseReader>(
        kMockResponseId, std::move(info), info_size, data, data_size));
  }

  std::unique_ptr<net::HttpResponseInfo> MakeMockResponseInfo() {
    auto info = std::make_unique<net::HttpResponseInfo>();
    info->request_time = base::Time::Now();
    info->response_time = base::Time::Now();
    info->was_cached = false;
    info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(kMockHeaders, base::size(kMockHeaders)));
    return info;
  }

  int GetResponseInfoSize(const net::HttpResponseInfo* info) {
    base::Pickle pickle;
    return PickleResponseInfo(&pickle, info);
  }

  int PickleResponseInfo(base::Pickle* pickle,
                         const net::HttpResponseInfo* info) {
    const bool kSkipTransientHeaders = true;
    const bool kTruncated = false;
    info->Persist(pickle, kSkipTransientHeaders, kTruncated);
    return pickle->size();
  }

  net::CompletionOnceCallback deletion_callback() {
    return base::BindOnce(&AppCacheServiceImplTest::OnDeleteAppCachesComplete,
                          base::Unretained(this));
  }

  const GURL kOriginURL;
  const url::Origin kOrigin;
  const GURL kManifestUrl;

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AppCacheServiceImpl> service_;
  int delete_result_;
  int delete_completion_count_;
};

TEST_F(AppCacheServiceImplTest, DeleteAppCachesForOrigin) {
  // Without giving mock storage simiulated info, should fail.
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback());
  EXPECT_EQ(0, delete_completion_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should succeed given an empty info collection.
  mock_storage()->SimulateGetAllInfo(
      base::MakeRefCounted<content::AppCacheInfoCollection>());
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback());
  EXPECT_EQ(0, delete_completion_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  auto info = base::MakeRefCounted<AppCacheInfoCollection>();

  // Should succeed given a non-empty info collection.
  blink::mojom::AppCacheInfo mock_manifest_1;
  blink::mojom::AppCacheInfo mock_manifest_2;
  blink::mojom::AppCacheInfo mock_manifest_3;
  mock_manifest_1.manifest_url = kOriginURL.Resolve("manifest1");
  mock_manifest_2.manifest_url = kOriginURL.Resolve("manifest2");
  mock_manifest_3.manifest_url = kOriginURL.Resolve("manifest3");
  std::vector<blink::mojom::AppCacheInfo> info_vector;
  info_vector.push_back(mock_manifest_1);
  info_vector.push_back(mock_manifest_2);
  info_vector.push_back(mock_manifest_3);
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info.get());
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback());
  EXPECT_EQ(0, delete_completion_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  // Should fail if storage fails to delete.
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info.get());
  mock_storage()->SimulateMakeGroupObsoleteFailure();
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback());
  EXPECT_EQ(0, delete_completion_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should complete with abort error if the service is deleted
  // prior to a delete completion.
  service_->DeleteAppCachesForOrigin(kOrigin, deletion_callback());
  EXPECT_EQ(0, delete_completion_count_);
  service_.reset();  // kill it
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_ABORTED, delete_result_);
  delete_completion_count_ = 0;

  // Let any tasks lingering from the sudden deletion run and verify
  // no other completion calls occur.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, delete_completion_count_);
}

TEST_F(AppCacheServiceImplTest, CheckAppCacheResponse) {
  // Check a non-existing manifest.
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  service_->CheckAppCacheResponse(kManifestUrl, 1, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response that looks good.
  // Nothing should be deleted.
  SetupMockGroup();
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  SetupMockReader(true, true, true);
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response for which there is no cache entry.
  // The group should get deleted.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId,
                                  kMissingResponseId);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response for which there is no manifest entry in a newer version
  // of the cache. Nothing should get deleted in this case.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMissingCacheId,
                                  kMissingResponseId);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_TRUE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with bad headers.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(false, true, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with bad data.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(true, false, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  // Check a response with truncated data.
  SetupMockGroup();
  service_->CheckAppCacheResponse(kManifestUrl, kMockCacheId, kMockResponseId);
  SetupMockReader(true, true, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, CountPendingHelpers());
  EXPECT_FALSE(IsGroupStored(kManifestUrl));
  ResetStorage();

  service_.reset();  // Clean up.
  base::RunLoop().RunUntilIdle();
}

// Just tests the backoff scheduling function, not the actual reinit function.
TEST_F(AppCacheServiceImplTest, ScheduleReinitialize) {
  const base::TimeDelta kNoDelay;
  const base::TimeDelta kOneSecond(base::TimeDelta::FromSeconds(1));
  const base::TimeDelta k30Seconds(base::TimeDelta::FromSeconds(30));
  const base::TimeDelta kOneHour(base::TimeDelta::FromHours(1));

  // Do things get initialized as expected?
  auto service = std::make_unique<AppCacheServiceImpl>(nullptr, nullptr);
  EXPECT_TRUE(service->last_reinit_time_.is_null());
  EXPECT_FALSE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(kNoDelay, service->next_reinit_delay_);

  // Do we see artifacts of the timer pending and such?
  service->ScheduleReinitialize();
  EXPECT_TRUE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(kNoDelay, service->reinit_timer_.GetCurrentDelay());
  EXPECT_EQ(k30Seconds, service->next_reinit_delay_);

  // Nothing should change if already scheduled
  service->ScheduleReinitialize();
  EXPECT_TRUE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(kNoDelay, service->reinit_timer_.GetCurrentDelay());
  EXPECT_EQ(k30Seconds, service->next_reinit_delay_);

  // Does the delay increase as expected?
  service->reinit_timer_.Stop();
  service->last_reinit_time_ = base::Time::Now() - kOneSecond;
  service->ScheduleReinitialize();
  EXPECT_TRUE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(k30Seconds, service->reinit_timer_.GetCurrentDelay());
  EXPECT_EQ(k30Seconds + k30Seconds, service->next_reinit_delay_);

  // Does the delay reset as expected?
  service->reinit_timer_.Stop();
  service->last_reinit_time_ = base::Time::Now() -
                               base::TimeDelta::FromHours(2);
  service->ScheduleReinitialize();
  EXPECT_TRUE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(kNoDelay, service->reinit_timer_.GetCurrentDelay());
  EXPECT_EQ(k30Seconds, service->next_reinit_delay_);

  // Does the delay max out as expected?
  service->reinit_timer_.Stop();
  service->last_reinit_time_ = base::Time::Now() - kOneSecond;
  service->next_reinit_delay_ = kOneHour;
  service->ScheduleReinitialize();
  EXPECT_TRUE(service->reinit_timer_.IsRunning());
  EXPECT_EQ(kOneHour, service->reinit_timer_.GetCurrentDelay());
  EXPECT_EQ(kOneHour, service->next_reinit_delay_);

  // Fine to delete while pending.
  service.reset(nullptr);
}



}  // namespace content
