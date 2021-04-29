// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_

#include <utility>
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

// SignedExchangePrefetchMetricRecorder records signed exchange prefetch and
// its usage metrics.
class CONTENT_EXPORT SignedExchangePrefetchMetricRecorder final
    : public base::RefCountedThreadSafe<SignedExchangePrefetchMetricRecorder> {
 public:
  explicit SignedExchangePrefetchMetricRecorder(
      const base::TickClock* tick_clock);

  void OnSignedExchangeNonPrefetch(const GURL& outer_url,
                                   base::Time response_time);
  void OnSignedExchangePrefetchFinished(const GURL& outer_url,
                                        base::Time response_time);

 private:
  friend class base::RefCountedThreadSafe<SignedExchangePrefetchMetricRecorder>;
  ~SignedExchangePrefetchMetricRecorder();

  void ScheduleFlushTimer();
  void OnFlushTimer();

  bool disabled_ = false;
  const base::TickClock* tick_clock_;

  using PrefetchEntries =
      base::flat_map<std::pair<GURL, base::Time /* response_time */>,
                     base::TimeTicks /* prefetch_time */>;
  PrefetchEntries recent_prefetch_entries_;

  std::unique_ptr<base::OneShotTimer> flush_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(SignedExchangePrefetchMetricRecorder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_PREFETCH_METRIC_RECORDER_H_
