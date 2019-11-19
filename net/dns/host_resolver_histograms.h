// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_HISTOGRAMS_H_
#define NET_DNS_HOST_RESOLVER_HISTOGRAMS_H_

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {
namespace dns_histograms {

// (Histogram names exported for testing.)

// The name of the histogram recording the outcome of ESNI-type
// transactions. Records successes, DnsTask-level
// timeouts, and the total number of started transactions.
NET_EXPORT_PRIVATE extern const char kEsniTransactionSuccessHistogram[];

// The name of the histogram recording the end-to-end aggregate duration
// of all non-ESNI transactions in DNS tasks with ESNI transactions.
NET_EXPORT_PRIVATE extern const char kNonEsniTotalTimeHistogram[];

// The names of the histograms recording the total end-to-end elapsed time
// (from task start) to the completion of successful ESNI transactions,
// the first for transactions made during DnsQueryType::UNSPECIFIED tasks
// and the second for transactions made during DnsQueryType::ESNI tasks.
NET_EXPORT_PRIVATE extern const char kEsniTimeHistogramForUnspecTasks[];
NET_EXPORT_PRIVATE extern const char kEsniTimeHistogramForEsniTasks[];

// The names of the histograms recording the absolute differences in end-to-end
// elapsed time between ESNI and non-ESNI transactions in
// DnsQueryType::UNSPECIFIED tasks. The first covers the case where the task's
// ESNI transaction completed last, the second the case where non-ESNI
// transactions completed last.
NET_EXPORT_PRIVATE extern const char kEsniVersusNonEsniWithEsniLonger[];
NET_EXPORT_PRIVATE extern const char kEsniVersusNonEsniWithNonEsniLonger[];

// Persisted to histograms. Do not relabel or delete entries.
enum class EsniSuccessOrTimeout {
  kSuccess = 0,
  kTimeout = 1,
  // To infer the number of failures, record the total
  // number of started ESNI transactions.
  kStarted = 2,
  kMaxValue = kStarted
};

// Logs |status| to |kEsniTransactionSuccessHistogram|.
void RecordEsniTransactionStatus(EsniSuccessOrTimeout status);

// Logs the difference between end-to-end ESNI and non-ESNI elapsed
// times, for UNSPECIFIED-with-ESNI tasks where all transactions
// complete successfully.
void RecordEsniVersusNonEsniTimes(base::TimeDelta esni_elapsed,
                                  base::TimeDelta non_esni_elapsed);

// Logs |elapsed| to the corresponding kEsniTime[...] histogram (see above).
void RecordEsniTimeForUnspecTask(base::TimeDelta elapsed);
void RecordNonEsniTimeForUnspecTask(base::TimeDelta elapsed);
void RecordEsniTimeForEsniTask(base::TimeDelta elapsed);

}  // namespace dns_histograms

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_HISTOGRAMS_H_
