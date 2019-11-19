// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_temporary_storage_evictor.h"

#include <stdint.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "storage/browser/quota/quota_macros.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

#define UMA_HISTOGRAM_MINUTES(name, sample) \
  UMA_HISTOGRAM_CUSTOM_TIMES(             \
      (name), (sample),                   \
      base::TimeDelta::FromMinutes(1),    \
      base::TimeDelta::FromDays(1), 50)

namespace {
const int64_t kMBytes = 1024 * 1024;
const double kUsageRatioToStartEviction = 0.7;
const int kThresholdOfErrorsToStopEviction = 5;
const int kHistogramReportIntervalMinutes = 60;
const double kDiskSpaceShortageAllowanceRatio = 0.5;
}

namespace storage {

QuotaTemporaryStorageEvictor::EvictionRoundStatistics::EvictionRoundStatistics()
    : in_round(false),
      is_initialized(false),
      diskspace_shortage_at_round(-1),
      usage_on_beginning_of_round(-1),
      usage_on_end_of_round(-1),
      num_evicted_origins_in_round(0) {
}

QuotaTemporaryStorageEvictor::QuotaTemporaryStorageEvictor(
    QuotaEvictionHandler* quota_eviction_handler,
    int64_t interval_ms)
    : quota_eviction_handler_(quota_eviction_handler),
      interval_ms_(interval_ms),
      timer_disabled_for_testing_(false) {
  DCHECK(quota_eviction_handler);
}

QuotaTemporaryStorageEvictor::~QuotaTemporaryStorageEvictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void QuotaTemporaryStorageEvictor::GetStatistics(
    std::map<std::string, int64_t>* statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(statistics);

  (*statistics)["errors-on-getting-usage-and-quota"] =
      statistics_.num_errors_on_getting_usage_and_quota;
  (*statistics)["evicted-origins"] =
      statistics_.num_evicted_origins;
  (*statistics)["eviction-rounds"] =
      statistics_.num_eviction_rounds;
  (*statistics)["skipped-eviction-rounds"] =
      statistics_.num_skipped_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::ReportPerRoundHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(round_statistics_.in_round);
  DCHECK(round_statistics_.is_initialized);

  base::Time now = base::Time::Now();
  UMA_HISTOGRAM_TIMES("Quota.TimeSpentToAEvictionRound",
                      now - round_statistics_.start_time);
  if (!time_of_end_of_last_round_.is_null())
    UMA_HISTOGRAM_MINUTES("Quota.TimeDeltaOfEvictionRounds",
                          now - time_of_end_of_last_round_);
  UMA_HISTOGRAM_MBYTES("Quota.DiskspaceShortage",
                       round_statistics_.diskspace_shortage_at_round);
  UMA_HISTOGRAM_MBYTES("Quota.EvictedBytesPerRound",
                       round_statistics_.usage_on_beginning_of_round -
                       round_statistics_.usage_on_end_of_round);
  UMA_HISTOGRAM_COUNTS_1M("Quota.NumberOfEvictedOriginsPerRound",
                          round_statistics_.num_evicted_origins_in_round);
}

void QuotaTemporaryStorageEvictor::ReportPerHourHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Statistics stats_in_hour(statistics_);
  stats_in_hour.subtract_assign(previous_statistics_);
  previous_statistics_ = statistics_;

  UMA_HISTOGRAM_COUNTS_1M("Quota.EvictedOriginsPerHour",
                          stats_in_hour.num_evicted_origins);
  UMA_HISTOGRAM_COUNTS_1M("Quota.EvictionRoundsPerHour",
                          stats_in_hour.num_eviction_rounds);
  UMA_HISTOGRAM_COUNTS_1M("Quota.SkippedEvictionRoundsPerHour",
                          stats_in_hour.num_skipped_eviction_rounds);
}

void QuotaTemporaryStorageEvictor::OnEvictionRoundStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (round_statistics_.in_round)
    return;
  round_statistics_.in_round = true;
  round_statistics_.start_time = base::Time::Now();
  ++statistics_.num_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::OnEvictionRoundFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if skipped round
  if (round_statistics_.num_evicted_origins_in_round) {
    ReportPerRoundHistogram();
    time_of_end_of_last_nonskipped_round_ = base::Time::Now();
  } else {
    ++statistics_.num_skipped_eviction_rounds;
  }
  // Reset stats for next round.
  round_statistics_ = EvictionRoundStatistics();
}

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoReset<bool> auto_reset(&timer_disabled_for_testing_, false);
  StartEvictionTimerWithDelay(0);

  if (histogram_timer_.IsRunning())
    return;

  histogram_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(kHistogramReportIntervalMinutes),
      this, &QuotaTemporaryStorageEvictor::ReportPerHourHistogram);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(
    int64_t delay_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (eviction_timer_.IsRunning() || timer_disabled_for_testing_)
    return;
  eviction_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay_ms),
                        this, &QuotaTemporaryStorageEvictor::ConsiderEviction);
}

void QuotaTemporaryStorageEvictor::ConsiderEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnEvictionRoundStarted();
  quota_eviction_handler_->GetEvictionRoundInfo(
      base::BindOnce(&QuotaTemporaryStorageEvictor::OnGotEvictionRoundInfo,
                     weak_factory_.GetWeakPtr()));
}

void QuotaTemporaryStorageEvictor::OnGotEvictionRoundInfo(
    blink::mojom::QuotaStatusCode status,
    const QuotaSettings& settings,
    int64_t available_space,
    int64_t total_space,
    int64_t current_usage,
    bool current_usage_is_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(current_usage, 0);

  // Note: if there is no storage pressure, |current_usage|
  // may not be fully calculated and may be 0.

  if (status != blink::mojom::QuotaStatusCode::kOk)
    ++statistics_.num_errors_on_getting_usage_and_quota;

  int64_t usage_overage = std::max(
      INT64_C(0),
      current_usage - static_cast<int64_t>(settings.pool_size *
                                           kUsageRatioToStartEviction));
  int64_t diskspace_shortage =
      std::max(INT64_C(0),
               settings.should_remain_available - available_space);
  DCHECK(current_usage_is_complete || diskspace_shortage == 0);

  // If we're using so little that freeing all of it wouldn't help,
  // don't let the low space condition cause us to delete it all.
  if (current_usage < static_cast<int64_t>(diskspace_shortage *
                                           kDiskSpaceShortageAllowanceRatio)) {
    diskspace_shortage = 0;
  }

  if (!round_statistics_.is_initialized) {
    round_statistics_.diskspace_shortage_at_round = diskspace_shortage;
    round_statistics_.usage_on_beginning_of_round = current_usage;
    round_statistics_.is_initialized = true;
  }
  round_statistics_.usage_on_end_of_round = current_usage;

  int64_t amount_to_evict = std::max(usage_overage, diskspace_shortage);
  if (status == blink::mojom::QuotaStatusCode::kOk && amount_to_evict > 0) {
    // Space is getting tight. Get the least recently used origin and continue.
    // TODO(michaeln): if the reason for eviction is low physical disk space,
    // make 'unlimited' origins subject to eviction too.
    quota_eviction_handler_->GetEvictionOrigin(
        blink::mojom::StorageType::kTemporary, settings.pool_size,
        base::BindOnce(&QuotaTemporaryStorageEvictor::OnGotEvictionOrigin,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // No action required, sleep for a while and check again later.
  if (statistics_.num_errors_on_getting_usage_and_quota <
      kThresholdOfErrorsToStopEviction) {
    StartEvictionTimerWithDelay(interval_ms_);
  } else {
    // TODO(dmikurube): Add error handling for the case status is not OK.
    // TODO(dmikurube): Try restarting eviction after a while.
    LOG(WARNING) << "Stopped eviction of temporary storage due to errors";
  }

  OnEvictionRoundFinished();
}

void QuotaTemporaryStorageEvictor::OnGotEvictionOrigin(
    const base::Optional<url::Origin>& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!origin.has_value()) {
    StartEvictionTimerWithDelay(interval_ms_);
    OnEvictionRoundFinished();
    return;
  }

  DCHECK(!origin->GetURL().is_empty());

  quota_eviction_handler_->EvictOriginData(
      *origin, blink::mojom::StorageType::kTemporary,
      base::BindOnce(&QuotaTemporaryStorageEvictor::OnEvictionComplete,
                     weak_factory_.GetWeakPtr()));
}

void QuotaTemporaryStorageEvictor::OnEvictionComplete(
    blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Just calling ConsiderEviction() or StartEvictionTimerWithDelay() here is
  // ok.  No need to deal with the case that all of the Delete operations fail
  // for a certain origin.  It doesn't result in trying to evict the same
  // origin permanently.  The evictor skips origins which had deletion errors
  // a few times.

  if (status == blink::mojom::QuotaStatusCode::kOk) {
    ++statistics_.num_evicted_origins;
    ++round_statistics_.num_evicted_origins_in_round;
    // We many need to get rid of more space so reconsider immediately.
    ConsiderEviction();
  } else {
    // Sleep for a while and retry again until we see too many errors.
    StartEvictionTimerWithDelay(interval_ms_);
    OnEvictionRoundFinished();
  }
}

}  // namespace storage
