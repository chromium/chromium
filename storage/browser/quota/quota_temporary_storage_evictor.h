// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

class QuotaEvictionHandler;
enum class QuotaError;
struct QuotaSettings;

class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaTemporaryStorageEvictor {
 public:
  struct Statistics {
    int64_t num_errors_on_getting_usage_and_quota = 0;
    int64_t num_evicted_buckets = 0;
    int64_t num_eviction_rounds = 0;
    int64_t num_skipped_eviction_rounds = 0;

    void subtract_assign(const Statistics& rhs) {
      num_errors_on_getting_usage_and_quota -=
          rhs.num_errors_on_getting_usage_and_quota;
      num_evicted_buckets -= rhs.num_evicted_buckets;
      num_eviction_rounds -= rhs.num_eviction_rounds;
      num_skipped_eviction_rounds -= rhs.num_skipped_eviction_rounds;
    }
  };

  struct EvictionRoundStatistics {
    bool in_round = false;
    bool is_initialized = false;

    base::Time start_time;
    int64_t diskspace_shortage_at_round = -1;

    int64_t usage_on_beginning_of_round = -1;
    int64_t usage_on_end_of_round = -1;
    int64_t num_evicted_buckets = 0;
  };

  QuotaTemporaryStorageEvictor(QuotaEvictionHandler* quota_eviction_handler,
                               int64_t interval_ms);

  QuotaTemporaryStorageEvictor(const QuotaTemporaryStorageEvictor&) = delete;
  QuotaTemporaryStorageEvictor& operator=(const QuotaTemporaryStorageEvictor&) =
      delete;

  ~QuotaTemporaryStorageEvictor();

  void GetStatistics(std::map<std::string, int64_t>* statistics);
  void ReportPerRoundHistogram();
  void ReportPerHourHistogram();
  void Start();

  bool in_round() const { return round_statistics_.in_round; }

 private:
  friend class QuotaTemporaryStorageEvictorTest;

  void StartEvictionTimerWithDelay(int64_t delay_ms);
  void ConsiderEviction();
  void OnEvictedExpiredBuckets(blink::mojom::QuotaStatusCode status);
  void OnGotEvictionRoundInfo(blink::mojom::QuotaStatusCode status,
                              const QuotaSettings& settings,
                              int64_t available_space,
                              int64_t total_space,
                              int64_t current_usage,
                              bool current_usage_is_complete);
  void OnGotEvictionBuckets(const std::set<BucketLocator>& buckets);
  void OnEvictionComplete(int expected_evicted_buckets,
                          int actual_evicted_buckets);

  void OnEvictionRoundStarted();
  void OnEvictionRoundFinished();

  // Not owned; quota_eviction_handler owns us.
  raw_ptr<QuotaEvictionHandler> quota_eviction_handler_;

  Statistics statistics_;
  Statistics previous_statistics_;
  EvictionRoundStatistics round_statistics_;
  base::Time time_of_end_of_last_nonskipped_round_;

  int64_t interval_ms_;
  bool timer_disabled_for_testing_ = false;
  base::RepeatingClosure on_round_finished_for_testing_;

  base::OneShotTimer eviction_timer_;
  base::RepeatingTimer histogram_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<QuotaTemporaryStorageEvictor> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
