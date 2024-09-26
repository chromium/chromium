// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::mojom::StorageType;

namespace storage {

class QuotaTemporaryStorageEvictorTest;

namespace {

struct EvictionBucket {
  BucketLocator locator;
  int64_t usage;
};

class MockQuotaEvictionHandler : public QuotaEvictionHandler {
 public:
  void EvictExpiredBuckets(StatusCallback done) override {
    ++evict_expired_buckets_count_;
    std::move(done).Run(blink::mojom::QuotaStatusCode::kOk);
  }

  void EvictBucketData(const std::set<BucketLocator>& buckets,
                       base::OnceCallback<void(int)> callback) override {
    for (auto bucket : buckets) {
      int64_t bucket_usage = EnsureBucketRemoved(bucket);
      if (bucket_usage >= 0) {
        available_space_ += bucket_usage;
      }
    }
    std::move(callback).Run(buckets.size());
  }

  void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) override {
    if (!task_for_get_usage_and_quota_.is_null())
      task_for_get_usage_and_quota_.Run();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), blink::mojom::QuotaStatusCode::kOk,
                       settings_, available_space_, available_space_ * 2,
                       GetUsage(), true));
  }

  void GetEvictionBuckets(int64_t target_usage,
                          GetBucketsCallback callback) override {
    int64_t usage_evicted = 0;
    std::set<BucketLocator> buckets_to_evict;
    for (const BucketLocator& bucket : bucket_order_) {
      buckets_to_evict.insert(bucket);
      usage_evicted += buckets_[bucket.id];
      if (usage_evicted >= target_usage) {
        break;
      }
    }

    std::move(callback).Run(buckets_to_evict);
  }

  int64_t GetUsage() const {
    int64_t total_usage = 0;
    for (const auto& bucket_usage_pair : buckets_)
      total_usage += bucket_usage_pair.second;
    return total_usage;
  }

  const QuotaSettings& settings() const { return settings_; }
  void SetPoolSize(int64_t pool_size) {
    settings_.pool_size = pool_size;
    settings_.per_storage_key_quota = pool_size / 5;
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
  size_t get_evict_expired_buckets_count() const {
    return evict_expired_buckets_count_;
  }

  // Simulates an access to `bucket`. It reorders the internal LRU list.
  // It internally uses AddBucket().
  void AccessBucket(const BucketLocator& bucket) {
    const auto& it = buckets_.find(bucket.id);
    EXPECT_TRUE(buckets_.end() != it);
    AddBucket(bucket, it->second);
  }

  // Simulates adding or overwriting the `bucket` to the internal bucket set
  // with the `usage`.  It also adds or moves the `bucket` to the
  // end of the LRU list.
  void AddBucket(const BucketLocator& bucket, int64_t usage) {
    EnsureBucketRemoved(bucket);
    bucket_order_.push_back(bucket);
    buckets_[bucket.id] = usage;
  }

  bool HasBucket(const EvictionBucket& bucket) {
    return base::Contains(buckets_, bucket.locator.id);
  }

 private:
  int64_t EnsureBucketRemoved(const BucketLocator& bucket) {
    int64_t bucket_usage;
    if (!base::Contains(buckets_, bucket.id))
      return -1;
    else
      bucket_usage = buckets_[bucket.id];

    buckets_.erase(bucket.id);
    bucket_order_.remove(bucket);
    return bucket_usage;
  }

  QuotaSettings settings_;
  int64_t available_space_ = 0;
  std::list<BucketLocator> bucket_order_;
  std::map<BucketId, int64_t> buckets_;
  // The number of times `EvictExpiredBuckets()` has been called.
  size_t evict_expired_buckets_count_ = 0;

  base::RepeatingClosure task_for_get_usage_and_quota_;
};

}  // namespace

class QuotaTemporaryStorageEvictorTest : public testing::Test {
 public:
  QuotaTemporaryStorageEvictorTest()
      : num_get_usage_and_quota_for_eviction_(0) {}

  QuotaTemporaryStorageEvictorTest(const QuotaTemporaryStorageEvictorTest&) =
      delete;
  QuotaTemporaryStorageEvictorTest& operator=(
      const QuotaTemporaryStorageEvictorTest&) = delete;

  void SetUp() override {
    quota_eviction_handler_ = std::make_unique<MockQuotaEvictionHandler>();

    // Run multiple evictions in a single RunUntilIdle() when interval_ms == 0
    temporary_storage_evictor_ = std::make_unique<QuotaTemporaryStorageEvictor>(
        quota_eviction_handler_.get(), 0);
  }

  void TearDown() override {
    temporary_storage_evictor_.reset();
    quota_eviction_handler_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void TaskForRepeatedEvictionTest(
      const std::pair<std::optional<BucketLocator>, int64_t>&
          bucket_to_be_added,
      const std::optional<BucketLocator> bucket_to_be_accessed,
      int expected_usage_after_first,
      int expected_usage_after_second) {
    EXPECT_GE(4, num_get_usage_and_quota_for_eviction_);
    switch (num_get_usage_and_quota_for_eviction_) {
      case 2:
        EXPECT_EQ(expected_usage_after_first,
                  quota_eviction_handler()->GetUsage());
        if (bucket_to_be_added.first.has_value())
          quota_eviction_handler()->AddBucket(*bucket_to_be_added.first,
                                              bucket_to_be_added.second);
        if (bucket_to_be_accessed.has_value())
          quota_eviction_handler()->AccessBucket(*bucket_to_be_accessed);
        break;
      case 3:
        EXPECT_EQ(expected_usage_after_second,
                  quota_eviction_handler()->GetUsage());
        temporary_storage_evictor()->timer_disabled_for_testing_ = true;
        break;
    }
    ++num_get_usage_and_quota_for_eviction_;
  }

  BucketLocator CreateBucket(const std::string& url, bool is_default) {
    return BucketLocator(bucket_id_generator_.GenerateNextId(),
                         blink::StorageKey::CreateFromStringForTesting(url),
                         blink::mojom::StorageType::kTemporary, is_default);
  }

  EvictionBucket CreateEvictionBucket(const std::string& url,
                                      int64_t usage,
                                      bool is_default = true) {
    return EvictionBucket{CreateBucket(url, is_default), usage};
  }

  EvictionBucket CreateAndAddBucket(const std::string& url,
                                    int64_t usage,
                                    bool is_default = true) {
    EvictionBucket bucket = CreateEvictionBucket(url, usage, is_default);
    quota_eviction_handler()->AddBucket(bucket.locator, bucket.usage);
    return bucket;
  }

  bool EvictorHasBuckets(const std::vector<EvictionBucket>& buckets) {
    int64_t total_usage = 0;
    for (const auto& bucket : buckets) {
      if (!quota_eviction_handler_->HasBucket(bucket)) {
        return false;
      }
      total_usage += bucket.usage;
    }
    return total_usage == quota_eviction_handler_->GetUsage();
  }

  void RunAnEvictionRound(bool schedule_next_round) {
    temporary_storage_evictor_->timer_disabled_for_testing_ =
        !schedule_next_round;
    base::RunLoop run_loop;
    quota_eviction_handler_->set_task_for_get_usage_and_quota(
        base::BindRepeating(
            [](int* num_get_usage_and_quota_for_eviction) {
              (*num_get_usage_and_quota_for_eviction)++;
            },
            base::Unretained(&num_get_usage_and_quota_for_eviction_)));
    set_on_round_finished_callback(run_loop.QuitClosure());
    temporary_storage_evictor()->Start();
    run_loop.Run();
  }

  bool RunAnEvictionPass() {
    base::RunLoop run_loop;
    quota_eviction_handler_->set_task_for_get_usage_and_quota(
        base::BindRepeating(
            [](base::RepeatingClosure quit_closure,
               int* num_get_usage_and_quota_for_eviction) {
              (*num_get_usage_and_quota_for_eviction)++;
              quit_closure.Run();
            },
            run_loop.QuitClosure(),
            base::Unretained(&num_get_usage_and_quota_for_eviction_)));
    set_on_round_finished_callback(run_loop.QuitClosure());
    run_loop.Run();
    return temporary_storage_evictor_->round_statistics_.in_round;
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

  void set_on_round_finished_callback(base::RepeatingClosure callback) {
    temporary_storage_evictor_->on_round_finished_for_testing_ = callback;
  }

  int num_get_usage_and_quota_for_eviction() const {
    return num_get_usage_and_quota_for_eviction_;
  }

  BucketId::Generator bucket_id_generator_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockQuotaEvictionHandler> quota_eviction_handler_;
  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  int num_get_usage_and_quota_for_eviction_;
  base::WeakPtrFactory<QuotaTemporaryStorageEvictorTest> weak_factory_{this};
};

TEST_F(QuotaTemporaryStorageEvictorTest, SimpleEvictionTest) {
  EXPECT_EQ(0U, quota_eviction_handler()->get_evict_expired_buckets_count());

  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.z.com", /*is_default=*/false), 3000);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.y.com", /*is_default=*/false), 200);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.x.com", /*is_default=*/false), 500);
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(3000 + 200 + 500, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(200 + 500, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(1U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

TEST_F(QuotaTemporaryStorageEvictorTest, MultipleEvictionTest) {
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.z.com", /*is_default=*/true), 20);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.y.com", /*is_default=*/true), 2900);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.x.com", /*is_default=*/true), 450);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.w.com", /*is_default=*/true), 400);
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);
  EXPECT_EQ(20 + 2900 + 450 + 400, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(450 + 400, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(1U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;
  const int64_t e_size = 275;

  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.d.com", /*is_default=*/false), d_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.c.com", /*is_default=*/false), c_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.b.com", /*is_default=*/false), b_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.a.com", /*is_default=*/false), a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(),
          std::make_pair(CreateBucket("http://www.e.com", /*is_default=*/false),
                         e_size),
          std::nullopt,
          // First round evicts d.
          initial_total_size - d_size,
          // Second round evicts c and b.
          initial_total_size - d_size + e_size - c_size - b_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size + e_size - c_size - b_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(4, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_buckets);
  EXPECT_EQ(2, statistics().num_eviction_rounds -
                   statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(4U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionSkippedTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;

  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.d.com", /*is_default=*/true), d_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.c.com", /*is_default=*/true), c_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.b.com", /*is_default=*/true), b_size);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.a.com", /*is_default=*/true), a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(), std::make_pair(std::nullopt, 0),
          std::nullopt, initial_total_size - d_size,
          initial_total_size - d_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size, quota_eviction_handler()->GetUsage());
  EXPECT_EQ(4, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(1, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds -
                   statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, RepeatedEvictionWithAccessBucketTest) {
  const int64_t a_size = 400;
  const int64_t b_size = 150;
  const int64_t c_size = 120;
  const int64_t d_size = 292;
  const int64_t initial_total_size = a_size + b_size + c_size + d_size;
  const int64_t e_size = 275;

  BucketLocator a_bucket =
      CreateBucket("http://www.a.com", /*is_default=*/true);
  BucketLocator b_bucket =
      CreateBucket("http://www.b.com", /*is_default=*/true);
  BucketLocator c_bucket =
      CreateBucket("http://www.c.com", /*is_default=*/true);
  BucketLocator d_bucket =
      CreateBucket("http://www.d.com", /*is_default=*/true);
  BucketLocator e_bucket =
      CreateBucket("http://www.e.com", /*is_default=*/true);

  quota_eviction_handler()->AddBucket(d_bucket, d_size);
  quota_eviction_handler()->AddBucket(c_bucket, c_size);
  quota_eviction_handler()->AddBucket(b_bucket, b_size);
  quota_eviction_handler()->AddBucket(a_bucket, a_size);
  quota_eviction_handler()->SetPoolSize(1000);
  quota_eviction_handler()->set_available_space(1000000000);
  quota_eviction_handler()->set_task_for_get_usage_and_quota(
      base::BindRepeating(
          &QuotaTemporaryStorageEvictorTest::TaskForRepeatedEvictionTest,
          weak_factory_.GetWeakPtr(), std::make_pair(e_bucket, e_size),
          c_bucket,
          // First round evicts d.
          initial_total_size - d_size,
          // Second round evicts b and a since c was accessed.
          initial_total_size - d_size + e_size - b_size - a_size));
  EXPECT_EQ(initial_total_size, quota_eviction_handler()->GetUsage());
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(initial_total_size - d_size + e_size - b_size - a_size,
            quota_eviction_handler()->GetUsage());
  EXPECT_EQ(4, num_get_usage_and_quota_for_eviction());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_buckets);
  EXPECT_EQ(2, statistics().num_eviction_rounds -
                   statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceNonEvictionTest) {
  // If we're using so little that evicting all of it wouldn't
  // do enough to alleviate a diskspace shortage, we don't evict.
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.z.com", /*is_default=*/true), 10);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.x.com", /*is_default=*/true), 20);
  quota_eviction_handler()->SetPoolSize(10000);
  quota_eviction_handler()->set_available_space(
      quota_eviction_handler()->settings().should_remain_available - 350);
  EXPECT_EQ(10 + 20, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(10 + 20, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(0, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(1, statistics().num_skipped_eviction_rounds);
}

TEST_F(QuotaTemporaryStorageEvictorTest, DiskSpaceEvictionTest) {
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.z.com", /*is_default=*/true), 294);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.y.com", /*is_default=*/true), 120);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.x.com", /*is_default=*/true), 150);
  quota_eviction_handler()->AddBucket(
      CreateBucket("http://www.w.com", /*is_default=*/true), 300);
  quota_eviction_handler()->SetPoolSize(10000);
  quota_eviction_handler()->set_available_space(
      quota_eviction_handler()->settings().should_remain_available - 350);
  EXPECT_EQ(294 + 120 + 150 + 300, quota_eviction_handler()->GetUsage());
  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(150 + 300, quota_eviction_handler()->GetUsage());

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);  // FIXME?
}

TEST_F(QuotaTemporaryStorageEvictorTest, CallingStartAfterEvictionScheduled) {
  // After running an eviction round and letting it schedule the next one, it
  // should immediately run the next one if you call Start instead of waiting
  // out the timer.
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);

  EvictionBucket bucket_z = CreateAndAddBucket("http://www.z.com", 20);
  EvictionBucket bucket_y = CreateAndAddBucket("http://www.y.com", 2900);
  EvictionBucket bucket_x = CreateAndAddBucket("http://www.x.com", 450);
  EvictionBucket bucket_w = CreateAndAddBucket("http://www.w.com", 400);
  EXPECT_TRUE(EvictorHasBuckets({bucket_z, bucket_y, bucket_x, bucket_w}));

  RunAnEvictionRound(/*schedule_next_round=*/true);
  EXPECT_TRUE(EvictorHasBuckets({bucket_x, bucket_w}));

  EvictionBucket bucket_v = CreateAndAddBucket("http://www.v.com", 2000);
  EvictionBucket bucket_u = CreateAndAddBucket("http://www.u.com", 400);
  EXPECT_TRUE(EvictorHasBuckets({bucket_x, bucket_w, bucket_v, bucket_u}));

  RunAnEvictionRound(/*schedule_next_round=*/false);
  EXPECT_TRUE(EvictorHasBuckets({bucket_w, bucket_v, bucket_u}));

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(3, statistics().num_evicted_buckets);
  EXPECT_EQ(2, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(2U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

TEST_F(QuotaTemporaryStorageEvictorTest,
       CallingStartImmediatelyAfterEvictionScheduled) {
  // After calling start to schedule an eviction round, calling start multiple
  // times before the eviction round begins shouldn't cause it to run multiple
  // times.
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);

  EvictionBucket bucket_z = CreateAndAddBucket("http://www.z.com", 20);
  EvictionBucket bucket_y = CreateAndAddBucket("http://www.y.com", 2900);
  EvictionBucket bucket_x = CreateAndAddBucket("http://www.x.com", 450);
  EvictionBucket bucket_w = CreateAndAddBucket("http://www.w.com", 400);
  EXPECT_TRUE(EvictorHasBuckets({bucket_z, bucket_y, bucket_x, bucket_w}));

  temporary_storage_evictor()->Start();
  temporary_storage_evictor()->Start();
  RunAnEvictionRound(/*schedule_next_round=*/false);
  EXPECT_TRUE(EvictorHasBuckets({bucket_x, bucket_w}));

  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(1U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

TEST_F(QuotaTemporaryStorageEvictorTest, CallingStartDuringEvictionRoutine) {
  // If we're in the middle of an eviction round, calling Start should do
  // nothing.
  quota_eviction_handler()->SetPoolSize(4000);
  quota_eviction_handler()->set_available_space(1000000000);

  EvictionBucket bucket_z = CreateAndAddBucket("http://www.z.com", 20);
  EvictionBucket bucket_y = CreateAndAddBucket("http://www.y.com", 2900);
  EvictionBucket bucket_x = CreateAndAddBucket("http://www.x.com", 450);
  EvictionBucket bucket_w = CreateAndAddBucket("http://www.w.com", 400);
  EXPECT_TRUE(EvictorHasBuckets({bucket_z, bucket_y, bucket_x, bucket_w}));

  disable_timer_for_testing();
  temporary_storage_evictor()->Start();
  while (RunAnEvictionPass()) {
    temporary_storage_evictor_->Start();
  }
  EXPECT_TRUE(EvictorHasBuckets({bucket_x, bucket_w}));

  EXPECT_EQ(1, num_get_usage_and_quota_for_eviction());
  EXPECT_EQ(0, statistics().num_errors_on_getting_usage_and_quota);
  EXPECT_EQ(2, statistics().num_evicted_buckets);
  EXPECT_EQ(1, statistics().num_eviction_rounds);
  EXPECT_EQ(0, statistics().num_skipped_eviction_rounds);
  EXPECT_EQ(1U, quota_eviction_handler()->get_evict_expired_buckets_count());
}

}  // namespace storage
