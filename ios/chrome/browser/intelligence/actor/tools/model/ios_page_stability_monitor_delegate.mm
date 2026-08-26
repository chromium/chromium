// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_monitor_delegate.h"

#import <memory>
#import <utility>
#import <vector>

#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/ios_page_stability_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "url/gurl.h"

namespace actor {

#pragma mark - Lifecycle

IOSPageStabilityMonitorDelegate::IOSPageStabilityMonitorDelegate()
    : IOSPageStabilityMonitorDelegate(TaskId(), nullptr) {}

IOSPageStabilityMonitorDelegate::IOSPageStabilityMonitorDelegate(
    TaskId task_id,
    base::WeakPtr<AggregatedJournal> journal)
    : PageStabilityMonitorDelegate(
          task_id,
          Thresholds{
              .timeout_delay = GetActorPageStabilityTimeout(),
              .min_wait = GetActorPageStabilityMinWait(),
              // Paint events are not tracked on iOS, so paint timeouts are
              // omitted.
              .initial_paint_timeout = base::TimeDelta(),
              .subsequent_paint_timeout = base::TimeDelta(),
          }),
      journal_(std::move(journal)) {}

IOSPageStabilityMonitorDelegate::~IOSPageStabilityMonitorDelegate() {
  if (!journal_ && async_journal_entry_) {
    async_journal_entry_->mark_as_terminated();
  }
}

#pragma mark - PageStabilityMonitorDelegate

void IOSPageStabilityMonitorDelegate::LogEvent(
    mojom::JournalEntryType type,
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  if (!journal_) {
    if (async_journal_entry_) {
      async_journal_entry_->mark_as_terminated();
      async_journal_entry_.reset();
    }
    return;
  }

  switch (type) {
    case mojom::JournalEntryType::kBegin:
      // Reset existing entry first to preserve begin/end ordering.
      async_journal_entry_.reset();
      async_journal_entry_ = journal_->CreatePendingAsyncEntry(
          /*url=*/GURL(), task_id(), journal_->AllocateDynamicTrackUUID(),
          event_name, std::move(details));
      break;
    case mojom::JournalEntryType::kEnd:
      if (async_journal_entry_) {
        async_journal_entry_->EndEntry(std::move(details));
        async_journal_entry_.reset();
      }
      break;
    case mojom::JournalEntryType::kInstant:
      // TODO(crbug.com/531873110): if async_journal_entry_ is set, we should
      // log to the pending entry instead of creating a new one.
      journal_->Log(/*url=*/GURL(), task_id(), event_name, std::move(details));
      break;
  }
}

std::unique_ptr<PageStabilityMetrics>
IOSPageStabilityMonitorDelegate::CreateMetrics() {
  return std::make_unique<IOSPageStabilityMetrics>();
}

}  // namespace actor
