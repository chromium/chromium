// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that this file only tests the basic behavior of the cache counter, as in
// when it counts and when not, when result is nonzero and when not. It does not
// test whether the result of the counting is correct. This is the
// responsibility of a lower layer, and is tested in
// DiskCacheBackendTest.CalculateSizeOfAllEntries in net_unittests.

#include "ios/chrome/browser/browsing_data/model/cache_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class CacheCounterTest : public PlatformTest {
 public:
  CacheCounterTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    context_getter_ = profile_->GetRequestContext();
  }

  ~CacheCounterTest() override {}

  ProfileIOS* profile() { return profile_.get(); }

  PrefService* prefs() { return profile_->GetPrefs(); }

  void SetCacheDeletionPref(bool value) {
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCache, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    prefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                        static_cast<int>(period));
  }

  // Create a cache entry on the IO thread.
  void CreateCacheEntry() {
    current_operation_ = OPERATION_ADD_ENTRY;
    next_step_ = STEP_GET_BACKEND;

    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CacheCounterTest::CacheOperationStep,
                                  base::Unretained(this), net::OK));
    WaitForIOThread();
  }

  // Clear the cache on the IO thread.
  void ClearCache() {
    current_operation_ = OPERATION_CLEAR_CACHE;
    next_step_ = STEP_GET_BACKEND;

    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CacheCounterTest::CacheOperationStep,
                                  base::Unretained(this), net::OK));
    WaitForIOThread();
  }

  // Wait for IO thread operations, such as cache creation, counting, writing,
  // deletion etc.
  void WaitForIOThread() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  // A callback method to be used by counters to report the result.
  void CountingCallback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    finished_ = result->Finished();

    if (finished_) {
      result_ =
          static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
              result.get())
              ->Value();
    }

    if (run_loop_ && finished_) {
      run_loop_->Quit();
    }
  }

  // Get the last reported counter result.
  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

 private:
  enum CacheOperation {
    OPERATION_ADD_ENTRY,
    OPERATION_CLEAR_CACHE,
  };

  enum CacheEntryCreationStep {
    STEP_GET_BACKEND,
    STEP_CLEAR_CACHE,
    STEP_CREATE_ENTRY,
    STEP_WRITE_DATA,
    STEP_CALLBACK,
    STEP_DONE
  };

  // One step in the process of creating a cache entry or clearing the cache.
  // Every step must be executed on IO thread after the previous one has
  // finished.
  void CacheOperationStep(int rv) {
    while (rv != net::ERR_IO_PENDING && next_step_ != STEP_DONE) {
      // The testing profile uses a memory cache which should not cause
      // any errors.
      DCHECK_GE(rv, 0);

      switch (next_step_) {
        case STEP_GET_BACKEND: {
          next_step_ = current_operation_ == OPERATION_ADD_ENTRY
                           ? STEP_CREATE_ENTRY
                           : STEP_CLEAR_CACHE;

          net::HttpCache* http_cache = context_getter_->GetURLRequestContext()
                                           ->http_transaction_factory()
                                           ->GetCache();

          std::tie(rv, backend_) = http_cache->GetBackend(base::BindRepeating(
              &CacheCounterTest::SaveBackendAndStep, base::Unretained(this)));
          break;
        }

        case STEP_CLEAR_CACHE: {
          next_step_ = STEP_CALLBACK;

          DCHECK(backend_);
          rv = backend_->DoomAllEntries(base::BindRepeating(
              &CacheCounterTest::CacheOperationStep, base::Unretained(this)));

          break;
        }

        case STEP_CREATE_ENTRY: {
          next_step_ = STEP_WRITE_DATA;

          DCHECK(backend_);
          disk_cache::EntryResult result = backend_->CreateEntry(
              "entry_key", net::HIGHEST,
              base::BindOnce(&CacheCounterTest::SaveEntryAndStep,
                             base::Unretained(this)));
          rv = result.net_error();
          if (rv != net::ERR_IO_PENDING) {
            entry_ = result.ReleaseEntry();
          }
          break;
        }

        case STEP_WRITE_DATA: {
          next_step_ = STEP_CALLBACK;

          std::string data = "entry data";
          auto buffer = base::MakeRefCounted<net::StringIOBuffer>(data);

          rv = entry_->WriteData(
              0, 0, buffer.get(), data.size(),
              base::BindRepeating(&CacheCounterTest::CacheOperationStep,
                                  base::Unretained(this)),
              true);

          break;
        }

        case STEP_CALLBACK: {
          next_step_ = STEP_DONE;

          if (current_operation_ == OPERATION_ADD_ENTRY) {
            entry_->Close();
          }

          web::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE, base::BindOnce(&CacheCounterTest::Callback,
                                        base::Unretained(this)));

          break;
        }

        case STEP_DONE: {
          NOTREACHED_IN_MIGRATION();
        }
      }
    }
  }

  void SaveBackendAndStep(net::HttpCache::GetBackendResult result) {
    backend_ = result.second;
    CacheOperationStep(result.first);
  }

  void SaveEntryAndStep(disk_cache::EntryResult result) {
    int rv = result.net_error();
    entry_ = result.ReleaseEntry();
    CacheOperationStep(rv);
  }

  // General completion callback.
  void Callback() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<ProfileIOS> profile_;

  CacheOperation current_operation_;
  CacheEntryCreationStep next_step_;

  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  raw_ptr<disk_cache::Backend> backend_;
  raw_ptr<disk_cache::Entry> entry_;

  bool finished_ = false;
  browsing_data::BrowsingDataCounter::ResultInt result_;
};

// Tests that for the empty cache, the result is zero.
TEST_F(CacheCounterTest, Empty) {
  CacheCounter counter(profile());
  counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that for a non-empty cache, the result is nonzero, and after deleting
// its contents, it's zero again. Note that the exact value of the result
// is tested in DiskCacheBackendTest.CalculateSizeOfAllEntries.
TEST_F(CacheCounterTest, BeforeAndAfterClearing) {
  CreateCacheEntry();

  CacheCounter counter(profile());
  counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();
  EXPECT_NE(0u, GetResult());

  ClearCache();
  counter.Restart();

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
TEST_F(CacheCounterTest, PrefChanged) {
  SetCacheDeletionPref(false);

  CacheCounter counter(profile());
  counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  SetCacheDeletionPref(true);

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counting is restarted when the time period changes. Currently,
// the results should be the same for every period. This is because the counter
// always counts the size of the entire cache, and it is up to the UI
// to interpret it as exact value or upper bound.
TEST_F(CacheCounterTest, PeriodChanged) {
  CreateCacheEntry();

  CacheCounter counter(profile());
  counter.Init(prefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForIOThread();
  browsing_data::BrowsingDataCounter::ResultInt result = GetResult();

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());
}

}  // namespace
