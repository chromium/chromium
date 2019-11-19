// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {
class QuotaTemporaryStorageEvictorTest;
}

namespace url {
class Origin;
}

namespace storage {

class QuotaEvictionHandler;
struct QuotaSettings;

class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaTemporaryStorageEvictor {
 public:
  struct Statistics {
    Statistics()
        : num_errors_on_getting_usage_and_quota(0),
          num_evicted_origins(0),
          num_eviction_rounds(0),
          num_skipped_eviction_rounds(0) {}
    int64_t num_errors_on_getting_usage_and_quota;
    int64_t num_evicted_origins;
    int64_t num_eviction_rounds;
    int64_t num_skipped_eviction_rounds;

    void subtract_assign(const Statistics& rhs) {
      num_errors_on_getting_usage_and_quota -=
          rhs.num_errors_on_getting_usage_and_quota;
      num_evicted_origins -= rhs.num_evicted_origins;
      num_eviction_rounds -= rhs.num_eviction_rounds;
      num_skipped_eviction_rounds -= rhs.num_skipped_eviction_rounds;
    }
  };

  struct EvictionRoundStatistics {
    EvictionRoundStatistics();

    bool in_round;
    bool is_initialized;

    base::Time start_time;
    int64_t diskspace_shortage_at_round;

    int64_t usage_on_beginning_of_round;
    int64_t usage_on_end_of_round;
    int64_t num_evicted_origins_in_round;
  };

  QuotaTemporaryStorageEvictor(QuotaEvictionHandler* quota_eviction_handler,
                               int64_t interval_ms);
  ~QuotaTemporaryStorageEvictor();

  void GetStatistics(std::map<std::string, int64_t>* statistics);
  void ReportPerRoundHistogram();
  void ReportPerHourHistogram();
  void Start();

 private:
  friend class content::QuotaTemporaryStorageEvictorTest;

  void StartEvictionTimerWithDelay(int64_t delay_ms);
  void ConsiderEviction();
  void OnGotEvictionRoundInfo(blink::mojom::QuotaStatusCode status,
                              const QuotaSettings& settings,
                              int64_t available_space,
                              int64_t total_space,
                              int64_t current_usage,
                              bool current_usage_is_complete);
  void OnGotEvictionOrigin(const base::Optional<url::Origin>& origin);
  void OnEvictionComplete(blink::mojom::QuotaStatusCode status);

  void OnEvictionRoundStarted();
  void OnEvictionRoundFinished();

  // Not owned; quota_eviction_handler owns us.
  QuotaEvictionHandler* quota_eviction_handler_;

  Statistics statistics_;
  Statistics previous_statistics_;
  EvictionRoundStatistics round_statistics_;
  base::Time time_of_end_of_last_nonskipped_round_;
  base::Time time_of_end_of_last_round_;

  int64_t interval_ms_;
  bool timer_disabled_for_testing_;

  base::OneShotTimer eviction_timer_;
  base::RepeatingTimer histogram_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<QuotaTemporaryStorageEvictor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictor);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
