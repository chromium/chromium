// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_temporary_storage_evictor.h"

#include <stdint.h>

#include <algorithm>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"

namespace {
constexpr int64_t kMBytes = 1024 * 1024;
constexpr double kUsageRatioToStartEviction = 0.7;
constexpr int kThresholdOfErrorsToStopEviction = 5;
constexpr int kHistogramReportIntervalMinutes = 60;
constexpr double kDiskSpaceShortageAllowanceRatio = 0.5;

void UmaHistogramMbytes(const std::string& name, int sample) {
  base::UmaHistogramCustomCounts(name, sample / kMBytes, 1,
                                 10 * 1024 * 1024 /* 10 TB */, 100);
}
}  // namespace

namespace storage {

QuotaTemporaryStorageEvictor::QuotaTemporaryStorageEvictor(
    QuotaEvictionHandler* quota_eviction_handler,
    int64_t interval_ms)
    : quota_eviction_handler_(quota_eviction_handler),
      interval_ms_(interval_ms) {
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
  (*statistics)["evicted-buckets"] = statistics_.num_evicted_buckets;
  (*statistics)["eviction-rounds"] = statistics_.num_eviction_rounds;
  (*statistics)["skipped-eviction-rounds"] =
      statistics_.num_skipped_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::ReportPerRoundHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(round_statistics_.in_round);
  DCHECK(round_statistics_.is_initialized);

  base::Time now = base::Time::Now();
  base::UmaHistogramTimes("Quota.TimeSpentToAEvictionRound",
                          now - round_statistics_.start_time);
  UmaHistogramMbytes("Quota.DiskspaceShortage",
                     round_statistics_.diskspace_shortage_at_round);
  UmaHistogramMbytes("Quota.EvictedBytesPerRound",
                     round_statistics_.usage_on_beginning_of_round -
                         round_statistics_.usage_on_end_of_round);
  base::UmaHistogramCounts1M("Quota.NumberOfEvictedBucketsPerRound",
                             round_statistics_.num_evicted_buckets);
}

void QuotaTemporaryStorageEvictor::ReportPerHourHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Statistics stats_in_hour(statistics_);
  stats_in_hour.subtract_assign(previous_statistics_);
  previous_statistics_ = statistics_;

  base::UmaHistogramCounts1M("Quota.EvictedBucketsPerHour",
                             stats_in_hour.num_evicted_buckets);
  base::UmaHistogramCounts1M("Quota.EvictionRoundsPerHour",
                             stats_in_hour.num_eviction_rounds);
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
  if (round_statistics_.num_evicted_buckets) {
    ReportPerRoundHistogram();
    time_of_end_of_last_nonskipped_round_ = base::Time::Now();
  } else {
    ++statistics_.num_skipped_eviction_rounds;
  }

  if (!on_round_finished_for_testing_.is_null()) {
    on_round_finished_for_testing_.Run();
  }

  // Reset stats for next round.
  round_statistics_ = EvictionRoundStatistics();
}

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't start while we're in a round.
  if (in_round()) {
    return;
  }

  // If we already have a round scheduled, just run it now.
  if (eviction_timer_.IsRunning()) {
    eviction_timer_.FireNow();
    return;
  }

  base::AutoReset<bool> auto_reset(&timer_disabled_for_testing_, false);
  StartEvictionTimerWithDelay(0);

  if (histogram_timer_.IsRunning())
    return;

  histogram_timer_.Start(FROM_HERE,
                         base::Minutes(kHistogramReportIntervalMinutes), this,
                         &QuotaTemporaryStorageEvictor::ReportPerHourHistogram);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(
    int64_t delay_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (eviction_timer_.IsRunning() || timer_disabled_for_testing_)
    return;
  eviction_timer_.Start(FROM_HERE, base::Milliseconds(delay_ms), this,
                        &QuotaTemporaryStorageEvictor::ConsiderEviction);
}

void QuotaTemporaryStorageEvictor::ConsiderEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!in_round());
  OnEvictionRoundStarted();
  quota_eviction_handler_->EvictExpiredBuckets(
      base::BindOnce(&QuotaTemporaryStorageEvictor::OnEvictedExpiredBuckets,
                     weak_factory_.GetWeakPtr()));
}

void QuotaTemporaryStorageEvictor::OnEvictedExpiredBuckets(
    blink::mojom::QuotaStatusCode status_code) {
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
      std::max(INT64_C(0), settings.should_remain_available - available_space);
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
    // TODO(michaeln): if the reason for eviction is low physical disk space,
    // make 'unlimited' storage keys subject to eviction too.
    quota_eviction_handler_->GetEvictionBuckets(
        amount_to_evict,
        base::BindOnce(&QuotaTemporaryStorageEvictor::OnGotEvictionBuckets,
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

void QuotaTemporaryStorageEvictor::OnGotEvictionBuckets(
    const std::set<BucketLocator>& buckets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buckets.empty()) {
    StartEvictionTimerWithDelay(interval_ms_);
    OnEvictionRoundFinished();
    return;
  }

  quota_eviction_handler_->EvictBucketData(
      buckets, base::BindOnce(&QuotaTemporaryStorageEvictor::OnEvictionComplete,
                              weak_factory_.GetWeakPtr(), buckets.size()));
}

void QuotaTemporaryStorageEvictor::OnEvictionComplete(
    int expected_evicted_buckets,
    int actual_evicted_buckets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  statistics_.num_evicted_buckets += actual_evicted_buckets;
  round_statistics_.num_evicted_buckets += actual_evicted_buckets;

  StartEvictionTimerWithDelay(interval_ms_);
  OnEvictionRoundFinished();
  return;
}

}  // namespace storage
