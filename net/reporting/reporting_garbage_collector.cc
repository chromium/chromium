// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_garbage_collector.h"

#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_cache_observer.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"

namespace net {

namespace {

class ReportingGarbageCollectorImpl : public ReportingGarbageCollector,
                                      public ReportingCacheObserver {
 public:
  explicit ReportingGarbageCollectorImpl(ReportingContext* context)
      : context_(context), timer_(std::make_unique<base::OneShotTimer>()) {
    context_->AddCacheObserver(this);
  }

  // ReportingGarbageCollector implementation:

  ~ReportingGarbageCollectorImpl() override {
    context_->RemoveCacheObserver(this);
  }

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer) override {
    timer_ = std::move(timer);
  }

  // ReportingObserver implementation:
  void OnReportsUpdated() override { EnsureTimerIsRunning(); }
  void OnEndpointsUpdatedForOrigin(
      const std::vector<ReportingEndpoint>& endpoints) override {
    EnsureTimerIsRunning();
  }

 private:
  // TODO(crbug.com/41430426): Garbage collect clients, reports with no matching
  // endpoints.
  void CollectGarbage() {
    base::TimeTicks now = context_->tick_clock().NowTicks();
    const ReportingPolicy& policy = context_->policy();

    base::flat_set<base::UnguessableToken> sources_to_remove =
        context_->cache()->GetExpiredSources();

    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> all_reports;
    context_->cache()->GetReports(&all_reports);

    std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
        failed_reports;
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
        expired_reports;
    for (const ReportingReport* report : all_reports) {
      if (report->attempts >= policy.max_report_attempts)
        failed_reports.push_back(report);
      else if (now - report->queued >= policy.max_report_age)
        expired_reports.push_back(report);
      else
        sources_to_remove.erase(report->reporting_source);
    }

    // Don't restart the timer on the garbage collector's own updates.
    context_->RemoveCacheObserver(this);
    context_->cache()->RemoveReports(failed_reports);
    context_->cache()->RemoveReports(expired_reports);
    for (const base::UnguessableToken& reporting_source : sources_to_remove) {
      context_->cache()->RemoveSourceAndEndpoints(reporting_source);
    }
    context_->AddCacheObserver(this);
  }

  void EnsureTimerIsRunning() {
    if (timer_->IsRunning())
      return;

    timer_->Start(FROM_HERE, context_->policy().garbage_collection_interval,
                  base::BindOnce(&ReportingGarbageCollectorImpl::CollectGarbage,
                                 base::Unretained(this)));
  }

  raw_ptr<ReportingContext> context_;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace

// static
std::unique_ptr<ReportingGarbageCollector> ReportingGarbageCollector::Create(
    ReportingContext* context) {
  return std::make_unique<ReportingGarbageCollectorImpl>(context);
}

ReportingGarbageCollector::~ReportingGarbageCollector() = default;

}  // namespace net
