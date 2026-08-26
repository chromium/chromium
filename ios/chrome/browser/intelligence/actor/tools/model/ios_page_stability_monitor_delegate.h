// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_MONITOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_MONITOR_DELEGATE_H_

#import <memory>
#import <string_view>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/page_stability_monitor_delegate.h"
#import "components/actor/core/task_id.h"
#import "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

// The iOS implementation of `PageStabilityMonitorDelegate`.
class IOSPageStabilityMonitorDelegate : public PageStabilityMonitorDelegate {
 public:
  IOSPageStabilityMonitorDelegate();
  IOSPageStabilityMonitorDelegate(TaskId task_id,
                                  base::WeakPtr<AggregatedJournal> journal);
  ~IOSPageStabilityMonitorDelegate() override;

  IOSPageStabilityMonitorDelegate(const IOSPageStabilityMonitorDelegate&) =
      delete;
  IOSPageStabilityMonitorDelegate& operator=(
      const IOSPageStabilityMonitorDelegate&) = delete;

 protected:
  // PageStabilityMonitorDelegate:
  void LogEvent(mojom::JournalEntryType type,
                std::string_view event_name,
                std::vector<mojom::JournalDetailsPtr> details) override;
  std::unique_ptr<PageStabilityMetrics> CreateMetrics() override;

 private:
  base::WeakPtr<AggregatedJournal> journal_;
  std::unique_ptr<AggregatedJournal::PendingAsyncEntry> async_journal_entry_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_IOS_PAGE_STABILITY_MONITOR_DELEGATE_H_
