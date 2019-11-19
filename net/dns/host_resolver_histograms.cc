// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_histograms.h"

#include "base/metrics/histogram_macros.h"

namespace net {

namespace dns_histograms {

const char kEsniTransactionSuccessHistogram[] =
    "Net.DNS.DnsTransaction.EsniUnspecTask.SuccessOrTimeout";

const char kNonEsniTotalTimeHistogram[] =
    "Net.DNS.DnsTransaction.EsniUnspecTask.NonEsniEndToEndElapsed";

const char kEsniTimeHistogramForEsniTasks[] =
    "Net.DNS.DnsTransaction.EsniTask.EsniTransactionEndToEndElapsed";

const char kEsniTimeHistogramForUnspecTasks[] =
    "Net.DNS.DnsTransaction.EsniUnspecTask.EsniTransactionEndToEndElapsed";

const char kEsniVersusNonEsniWithEsniLonger[] =
    "Net.DNS.DnsTransaction.EsniUnspecTask.EsniMinusNonEsni";

const char kEsniVersusNonEsniWithNonEsniLonger[] =
    "Net.DNS.DnsTransaction.EsniUnspecTask.NonEsniMinusEsni";

void RecordEsniTransactionStatus(EsniSuccessOrTimeout status) {
  UMA_HISTOGRAM_ENUMERATION(kEsniTransactionSuccessHistogram, status);
}

void RecordEsniTimeForEsniTask(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_LONG_TIMES_100(kEsniTimeHistogramForEsniTasks, elapsed);
}

void RecordEsniTimeForUnspecTask(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_LONG_TIMES_100(kEsniTimeHistogramForUnspecTasks, elapsed);
}

void RecordNonEsniTimeForUnspecTask(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_LONG_TIMES_100(kNonEsniTotalTimeHistogram, elapsed);
}

void RecordEsniVersusNonEsniTimes(base::TimeDelta esni_elapsed,
                                  base::TimeDelta non_esni_elapsed) {
  if (esni_elapsed > non_esni_elapsed) {
    UMA_HISTOGRAM_LONG_TIMES_100(kEsniVersusNonEsniWithEsniLonger,
                                 esni_elapsed - non_esni_elapsed);
  } else {
    // Choose this timer (arbitrarily) to record the case where the
    // times are equal; since they are obtained from TickClock::NowTicks(),
    // this should seldom occur.
    UMA_HISTOGRAM_LONG_TIMES_100(kEsniVersusNonEsniWithNonEsniLonger,
                                 non_esni_elapsed - esni_elapsed);
  }
}

}  // namespace dns_histograms

}  // namespace net
