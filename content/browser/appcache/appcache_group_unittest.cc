// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/test/task_environment.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_update_job.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace {

class TestAppCacheFrontend : public blink::mojom::AppCacheFrontend {
 public:
  TestAppCacheFrontend()
      : last_cache_id_(-1),
        last_status_(blink::mojom::AppCacheStatus::APPCACHE_STATUS_OBSOLETE) {}

  void CacheSelected(blink::mojom::AppCacheInfoPtr info) override {
    last_host_id_ = receivers_.current_context();
    last_cache_id_ = info->cache_id;
    last_status_ = info->status;
  }

  void EventRaised(blink::mojom::AppCacheEventID event_id) override {}

  void ErrorEventRaised(
      blink::mojom::AppCacheErrorDetailsPtr details) override {}

  void ProgressEventRaised(const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override {}

  void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override {}

  void SetSubresourceFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory)
      override {}

  mojo::PendingRemote<blink::mojom::AppCacheFrontend> Bind(
      const base::UnguessableToken& host_id) {
    mojo::PendingRemote<blink::mojom::AppCacheFrontend> result;
    receivers_.Add(this, result.InitWithNewPipeAndPassReceiver(), host_id);
    return result;
  }

  base::UnguessableToken last_host_id_;
  int64_t last_cache_id_;
  blink::mojom::AppCacheStatus last_status_;
  mojo::ReceiverSet<blink::mojom::AppCacheFrontend, base::UnguessableToken>
      receivers_;
};

}  // namespace

namespace content {

class TestUpdateObserver : public AppCacheGroup::UpdateObserver {
 public:
  TestUpdateObserver() : update_completed_(false), group_has_cache_(false) {}

  void OnUpdateComplete(AppCacheGroup* group) override {
    update_completed_ = true;
    group_has_cache_ = group->HasCache();
  }

  virtual void OnContentBlocked(AppCacheGroup* group) {}

  bool update_completed_;
  bool group_has_cache_;
};

class TestAppCacheHost : public AppCacheHost {
 public:
  TestAppCacheHost(const base::UnguessableToken& host_id,
                   TestAppCacheFrontend* frontend,
                   AppCacheServiceImpl* service)
      : AppCacheHost(host_id,
                     /*process_id=*/456,
                     /*render_frame_id=*/789,
                     frontend->Bind(host_id),
                     service),
        update_completed_(false) {}

  void OnUpdateComplete(AppCacheGroup* group) override {
    update_completed_ = true;
  }

  bool update_completed_;
};

class AppCacheGroupTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AppCacheGroupTest, AddRemoveCache) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(service.storage(),
                                                   GURL("http://foo.com"), 111);

  base::Time now = base::Time::Now();

  auto cache1 = base::MakeRefCounted<AppCache>(service.storage(), 111);
  cache1->set_complete(true);
  cache1->set_update_time(now);
  group->AddCache(cache1.get());
  EXPECT_EQ(cache1.get(), group->newest_complete_cache());

  // Adding older cache does not change newest complete cache.
  auto cache2 = base::MakeRefCounted<AppCache>(service.storage(), 222);
  cache2->set_complete(true);
  cache2->set_update_time(now - base::TimeDelta::FromDays(1));
  group->AddCache(cache2.get());
  EXPECT_EQ(cache1.get(), group->newest_complete_cache());

  // Adding newer cache does change newest complete cache.
  auto cache3 = base::MakeRefCounted<AppCache>(service.storage(), 333);
  cache3->set_complete(true);
  cache3->set_update_time(now + base::TimeDelta::FromDays(1));
  group->AddCache(cache3.get());
  EXPECT_EQ(cache3.get(), group->newest_complete_cache());

  // Adding cache with same update time uses one with larger ID.
  auto cache4 = base::MakeRefCounted<AppCache>(service.storage(), 444);
  cache4->set_complete(true);
  cache4->set_update_time(now + base::TimeDelta::FromDays(1));  // same as 3
  group->AddCache(cache4.get());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());

  // smaller id
  auto cache5 = base::MakeRefCounted<AppCache>(service.storage(), 55);
  cache5->set_complete(true);
  cache5->set_update_time(now + base::TimeDelta::FromDays(1));  // same as 4
  group->AddCache(cache5.get());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());  // no change

  // Old caches can always be removed.
  group->RemoveCache(cache1.get());
  EXPECT_FALSE(cache1->owning_group());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());  // newest unchanged

  // Remove rest of caches.
  group->RemoveCache(cache2.get());
  EXPECT_FALSE(cache2->owning_group());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());  // newest unchanged
  group->RemoveCache(cache3.get());
  EXPECT_FALSE(cache3->owning_group());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());  // newest unchanged
  group->RemoveCache(cache5.get());
  EXPECT_FALSE(cache5->owning_group());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());  // newest unchanged
  group->RemoveCache(cache4.get());                         // newest removed
  EXPECT_FALSE(cache4->owning_group());
  EXPECT_FALSE(group->newest_complete_cache());  // no more newest cache

  // Can remove newest cache if there are older caches.
  group->AddCache(cache1.get());
  EXPECT_EQ(cache1.get(), group->newest_complete_cache());
  group->AddCache(cache4.get());
  EXPECT_EQ(cache4.get(), group->newest_complete_cache());
  group->RemoveCache(cache4.get());  // remove newest
  EXPECT_FALSE(cache4->owning_group());
  EXPECT_FALSE(group->newest_complete_cache());  // newest removed
}

TEST_F(AppCacheGroupTest, CleanupUnusedGroup) {
  MockAppCacheService service;
  TestAppCacheFrontend frontend;
  AppCacheGroup* group =
      new AppCacheGroup(service.storage(), GURL("http://foo.com"), 111);

  auto host1_id = base::UnguessableToken::Create();
  AppCacheHost host1(host1_id,
                     /*process_id=*/1, /*render_frame_id=*/1,
                     frontend.Bind(host1_id), &service);
  auto host2_id = base::UnguessableToken::Create();
  AppCacheHost host2(host2_id,
                     /*process_id=*/2, /*render_frame_id=*/2,
                     frontend.Bind(host2_id), &service);

  base::Time now = base::Time::Now();

  AppCache* cache1 = new AppCache(service.storage(), 111);
  cache1->set_complete(true);
  cache1->set_update_time(now);
  group->AddCache(cache1);
  EXPECT_EQ(cache1, group->newest_complete_cache());

  host1.AssociateCompleteCache(cache1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(frontend.last_host_id_, host1.host_id());
  EXPECT_EQ(frontend.last_cache_id_, cache1->cache_id());
  EXPECT_EQ(frontend.last_status_,
            blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE);

  host2.AssociateCompleteCache(cache1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(frontend.last_host_id_, host2.host_id());
  EXPECT_EQ(frontend.last_cache_id_, cache1->cache_id());
  EXPECT_EQ(frontend.last_status_,
            blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE);

  AppCache* cache2 = new AppCache(service.storage(), 222);
  cache2->set_complete(true);
  cache2->set_update_time(now + base::TimeDelta::FromDays(1));
  group->AddCache(cache2);
  EXPECT_EQ(cache2, group->newest_complete_cache());

  // Unassociate all hosts from older cache.
  host1.AssociateNoCache(GURL());
  host2.AssociateNoCache(GURL());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(frontend.last_host_id_, host2.host_id());
  EXPECT_EQ(frontend.last_cache_id_, blink::mojom::kAppCacheNoCacheId);
  EXPECT_EQ(frontend.last_status_,
            blink::mojom::AppCacheStatus::APPCACHE_STATUS_UNCACHED);
}

TEST_F(AppCacheGroupTest, StartUpdate) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(service.storage(),
                                                   GURL("http://foo.com"), 111);

  // Set state to checking to prevent update job from executing fetches.
  group->update_status_ = AppCacheGroup::CHECKING;
  group->StartUpdate();
  AppCacheUpdateJob* update = group->update_job_;
  EXPECT_TRUE(update != nullptr);

  // Start another update, check that same update job is in use.
  group->StartUpdateWithHost(nullptr);
  EXPECT_EQ(update, group->update_job_);

  // Deleting the update should restore the group to AppCacheGroup::IDLE.
  delete update;
  EXPECT_TRUE(group->update_job_ == nullptr);
  EXPECT_EQ(AppCacheGroup::IDLE, group->update_status());
}

TEST_F(AppCacheGroupTest, CancelUpdate) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(service.storage(),
                                                   GURL("http://foo.com"), 111);

  // Set state to checking to prevent update job from executing fetches.
  group->update_status_ = AppCacheGroup::CHECKING;
  group->StartUpdate();
  AppCacheUpdateJob* update = group->update_job_;
  EXPECT_TRUE(update != nullptr);

  // Deleting the group should cancel the update.
  TestUpdateObserver observer;
  group->AddUpdateObserver(&observer);
  group = nullptr;  // causes group to be deleted
  EXPECT_TRUE(observer.update_completed_);
  EXPECT_FALSE(observer.group_has_cache_);
}

TEST_F(AppCacheGroupTest, QueueUpdate) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(service.storage(),
                                                   GURL("http://foo.com"), 111);

  // Set state to checking to prevent update job from executing fetches.
  group->update_status_ = AppCacheGroup::CHECKING;
  group->StartUpdate();
  EXPECT_TRUE(group->update_job_);

  // Pretend group's update job is terminating so that next update is queued.
  group->update_job_->internal_state_ = AppCacheUpdateJob::REFETCH_MANIFEST;
  EXPECT_TRUE(group->update_job_->IsTerminating());

  TestAppCacheFrontend frontend;
  TestAppCacheHost host(base::UnguessableToken::Create(), &frontend, &service);
  host.new_master_entry_url_ = GURL("http://foo.com/bar.txt");
  group->StartUpdateWithNewMasterEntry(&host, host.new_master_entry_url_);
  EXPECT_FALSE(group->queued_updates_.empty());

  group->AddUpdateObserver(&host);
  EXPECT_FALSE(group->FindObserver(&host, group->observers_));
  EXPECT_TRUE(group->FindObserver(&host, group->queued_observers_));

  // Delete update to cause it to complete. Verify no update complete notice
  // sent to host.
  delete group->update_job_;
  EXPECT_EQ(AppCacheGroup::IDLE, group->update_status_);
  EXPECT_FALSE(group->restart_update_task_.IsCancelled());
  EXPECT_FALSE(host.update_completed_);

  // Start another update. Cancels task and will run queued updates.
  group->update_status_ = AppCacheGroup::CHECKING;  // prevent actual fetches
  group->StartUpdate();
  EXPECT_TRUE(group->update_job_);
  EXPECT_TRUE(group->restart_update_task_.IsCancelled());
  EXPECT_TRUE(group->queued_updates_.empty());
  EXPECT_FALSE(group->update_job_->pending_master_entries_.empty());
  EXPECT_FALSE(group->FindObserver(&host, group->queued_observers_));
  EXPECT_TRUE(group->FindObserver(&host, group->observers_));

  // Delete update to cause it to complete. Verify host is notified.
  delete group->update_job_;
  EXPECT_EQ(AppCacheGroup::IDLE, group->update_status_);
  EXPECT_TRUE(group->restart_update_task_.IsCancelled());
  EXPECT_TRUE(host.update_completed_);
}

}  // namespace content
