// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/test/mock_storage_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::QuotaTemporaryStorageEvictor;
using blink::mojom::StorageType;

namespace content {

class QuotaTemporaryStorageEvictorTest;

namespace {

// TODO(crbug.com/889590): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}

class MockQuotaEvictionHandler : public storage::QuotaEvictionHandler {
 public:
  explicit MockQuotaEvictionHandler(QuotaTemporaryStorageEvictorTest* test)
      : available_space_(0),
        error_on_evict_origin_data_(false),
        error_on_get_usage_and_quota_(false) {}

  void EvictOriginData(const url::Origin& origin,
                       StorageType type,
                       storage::StatusCallback callback) override {
    if (error_on_evict_origin_data_) {
      std::move(callback).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
      return;
    }
    int64_t origin_usage = EnsureOriginRemoved(origin);
    if (origin_usage >= 0)
      available_space_ += origin_usage;
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
  }

  void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) override {
    if (error_on_get_usage_and_quota_) {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorAbort,
                              storage::QuotaSettings(), 0, 0, 0, false);
      return;
    }
    if (!task_for_get_usage_and_quota_.is_null())
      task_for_get_usage_and_quota_.Run();
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, settings_,
                            available_space_, available_space_ * 2, GetUsage(),
                            true);
  }

  void GetEvictionOrigin(StorageType type,
                         const std::set<url::Origin>& exceptions,
                         int64_t global_quota,
                         storage::GetOriginCallback callback) override {
    if (origin_order_.empty())
      std::move(callback).Run(base::nullopt);
    else
      std::move(callback).Run(origin_order_.front());
  }

  int64_t GetUsage() const {
    int64_t total_usage = 0;
    for (const auto& origin_usage_pair : origins_)
      total_usage += origin_usage_pair.second;
    return total_usage;
  }

  const storage::QuotaSettings& settings() const { return settings_; }
  void SetPoolSize(int64_t pool_size) {
    settings_.pool_size = pool_size;
    settings_.per_host_quota = pool_size / 5;
    settings_.should_remain_available = pool_size / 5;
    settings_.must_remain_available = pool_size / 100;
    settings_.refresh_interval = base::TimeDelta::Max();
  }
  void set_available_space(int64_t available_space) {
    available_space_ = available_space;
  }
  void set_task_for_get_usage_and_quota(base::RepeatingClosure task) {
    task_for_get_usage_and_quota_ = std::move(task);
  }
  void set_error_on_evict_origin_data(bool error_on_evict_origin_data) {
    error_on_evict_origin_data_ = error_on_evict_origin_data;
  }
  void set_error_on_get_usage_and_quota(bool error_on_get_usage_and_quota) {
    error_on_get_usage_and_quota_ = error_on_get_usage_and_quota;
  }

  // Simulates an access to |origin|.  It reorders the internal LRU list.
  // It internally uses AddOrigin().
  void AccessOrigin(const url::Origin& origin) {
    const auto& found = origins_.find(origin);
    EXPECT_TRUE(origins_.end() != found);
    AddOrigin(origin, found->second);
  }

  // Simulates adding or overwriting the |origin| to the internal origin set
  // with the |usage|.  It also adds or moves the |origin| to the end of the
  // LRU list.
  void AddOrigin(const url::Origin& origin, int64_t usage) {
    EnsureOriginRemoved(origin);
    origin_order_.push_back(origin);
    origins_[origin] = usage;
  }

 private:
  int64_t EnsureOriginRemoved(const url::Origin& origin) {
    int64_t origin_usage;
    if (!base::ContainsKey(origins_, origin))
      return -1;
    else
      origin_usage = origins_[origin];

    origins_.erase(origin);
    origin_order_.remove(origin);
    return origin_usage;
  }

  storage::QuotaSettings settings_;
  int64_t available_space_;
  std::list<url::Origin> origin_order_;
  std::map<url::Origin, int64_t> origins_;
  bool error_on_evict_origin_data_;
  bool error_on_get_usage_and_quota_;

  base::Closure task_for_get_usage_and_quota_;
};

}  // namespace

class QuotaTemporaryStorageEvictorTest : public testing::Test {
 public:
  QuotaTemporaryStorageEvictorTest()
      : num_get_usage_and_quota_for_eviction_(0),
        weak_factory_(this) {}

  void SetUp() override {
    quota_eviction_handler_.reset(new MockQuotaEvictionHandler(this));

    // Run multiple evictions in a single RunUntilIdle() when interval_ms == 0
    temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
        quota_eviction_handler_.get(), 0));
  }

  void TearDown() override {
    temporary_storage_evictor_.reset();
    quota_eviction_handler_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void TaskForRepeatedEvictionTest(
      const std::pair<base::Optional<url::Origin>, int64_t>& origin_to_be_added,
      const base::Optional<url::Origin>& origin_to_be_accessed,
      int expected_usage_after_first,
      int expected_usage_after_second) {
    EXPECT_GE(4, num_get_usage_and_quota_for_eviction_);
    switch (num_get_usage_and_quota_for_eviction_) {
      case 2:
        EXPECT_EQ(expected_usage_after_first,
                  quota_eviction_handler()->GetUsage());
        if (origin_to_be_added.first.has_value())
          quota_eviction_handler()->AddOrigin(*origin_to_be_added.first,
                                              origin_to_be_added.second);
        if (origin_to_be_accessed.has_value())
          quota_eviction_handler()->AccessOrigin(*origin_to_be_accessed);
        break;
      case 3:
        EXPECT_EQ(expected_usage_after_second,
                  quota_eviction_handler()->GetUsage());
        temporary_storage_evictor()->timer_disabled_for_testing_ = true;
        break;
    }
    ++num_get_usage_and_quota_for_eviction_;
  }

 protected:
  MockQuotaEvictionHandler* quota_eviction_handler() const {
    return static_cast<MockQuotaEvictionHandler*>(
        quota_eviction_handler_.get());
  }

  QuotaTemporaryStorageEvictor* temporary_storage_evictor() const {
    return temporary_storage_evictor_.get();
  }

  const QuotaTemporaryStorageEvictor::Statistics& statistics() const {
    return temporary_storage_evictor()->statistics_;
  }

  void disable_timer_for_testing() const {
    temporary_storage_evictor_->timer_disabled_for_testing_ = true;
  }

  int num_get_usage_and_quota_for_eviction() const {
    return num_get_usage_and_quota_for_eviction_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<MockQuotaEvictionHandler> quota_eviction_handler_;
  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  int num_get_usage_and_quota_for_eviction_;
  base::WeakPtrFactory<QuotaTemporaryStorageEvictorTest> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictorTest);
};

TEST_F(QuotaTemporaryStorageEvictorTest, SimpleEvictionTest) {
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.z.com"), 3000);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.y.com"), 200);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.x.com"), 500);
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(3000 + 200 + 500, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(200 + 500, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, MultipleEvictionTest) {
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.z.com"), 20);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.y.com"), 2900);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.x.com"), 450);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.w.com"), 400);
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(20 + 2900 + 450 + 400, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(450 + 400, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;
  const int64_t e_size = 275;

  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.a.com"), a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(),
          std::make_pair(ToOrigin("http://www.e.com"), e_size), base::nullopt,
          initial_total_size - d_size,
          initial_total_size - d_size + e_size - c_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size + e_size - c_size - b_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(5, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_origins);
  EXPECT_EQ(2, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionSkippedTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;

  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.a.com"), a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(), std::make_pair(base::nullopt, 0),
          base::nullopt, initial_total_size - d_size,
          initial_total_size - d_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  // disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size, quota_eviction_handler()->GetUsage());
  EXPECT_EQ(4, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_origins);
  EXPECT_EQ(3, statistics().num_eviction_rounds);
  EXPECT_EQ(2, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionWithAccessOriginTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;
  const int64_t e_size = 275;

  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.d.com"), d_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.c.com"), c_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.b.com"), b_size);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.a.com"), a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(),
          std::make_pair(ToOrigin("http://www.e.com"), e_size),
          ToOrigin("http://www.c.com"), initial_total_size - d_size,
          initial_total_size - d_size + e_size - b_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size + e_size - b_size - a_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(5, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_origins);
  EXPECT_EQ(2, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceNonEvictionTest) {
  // If we're using so little that evicting all of it wouldn't
  // do enough to alleviate a diskspace shortage, we don't evict.
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.z.com"), 10);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.x.com"), 20);
  quota_eviction_handler()->SetPoolSize(10000);
  quota_eviction_handler()->set_available_space(
      quota_eviction_handler()->settings().should_remain_available - 350);
  EXPECT_EQ(10 + 20, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(10 + 20, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(0, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(1, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceEvictionTest) {
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.z.com"), 294);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.y.com"), 120);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.x.com"), 150);
  quota_eviction_handler()->AddOrigin(ToOrigin("http://www.w.com"), 300);
  quota_eviction_handler()->SetPoolSize(10000);
  quota_eviction_handler()->set_available_space(
      quota_eviction_handler()->settings().should_remain_available - 350);
  EXPECT_EQ(294 + 120 + 150 + 300, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(150 + 300, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_evicting_origin);
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_origins);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds); // FIXME?
}

}  // namespace content
