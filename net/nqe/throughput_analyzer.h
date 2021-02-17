// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_THROUGHPUT_ANALYZER_H_
#define NET_NQE_THROUGHPUT_ANALYZER_H_

#include <stdint.h>

#include <unordered_map>
#include <unordered_set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"

namespace {
typedef base::RepeatingCallback<void(int32_t)> ThroughputObservationCallback;
}

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

namespace net {

class NetworkQualityEstimatorParams;
class NetworkQualityEstimator;
class URLRequest;

namespace nqe {

namespace internal {

// Makes throughput observations. Polls NetworkActivityMonitor
// (TrafficStats on Android) to count number of bits received over throughput
// observation windows in accordance with the following rules:
// (1) A new window of observation begins any time a URL request header is
//     about to be sent, or a request completes or is destroyed.
// (2) A request is "active" if its headers are sent, but it hasn't completed,
//     and "local" if destined to local host. If at any time during a
//     throughput observation window there is an active, local request, the
//     window is discarded.
// (3) If less than 32KB is received over the network during a window of
//     observation, that window is discarded.
class NET_EXPORT_PRIVATE ThroughputAnalyzer {
 public:
  // |throughput_observation_callback| is called on the |task_runner| when
  // |this| has a new throughput observation.
  // |use_local_host_requests_for_tests| should only be true when testing
  // against local HTTP server and allows the requests to local host to be
  // used for network quality estimation. |use_smaller_responses_for_tests|
  // should only be true when testing, and allows the responses smaller than
  // |kMinTransferSizeInBits| or shorter than
  // |kMinRequestDurationMicroseconds| to be used for network quality
  // estimation.
  // Virtualized for testing.
  ThroughputAnalyzer(
      const NetworkQualityEstimator* network_quality_estimator,
      const NetworkQualityEstimatorParams* params,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ThroughputObservationCallback throughput_observation_callback,
      const base::TickClock* tick_clock,
      const NetLogWithSource& net_log);
  virtual ~ThroughputAnalyzer();

  // Notifies |this| that the headers of |request| are about to be sent.
  void NotifyStartTransaction(const URLRequest& request);

  // Notifies |this| that unfiltered bytes have been read for |request|.
  void NotifyBytesRead(const URLRequest& request);

  // Notifies |this| that |request| has completed.
  void NotifyRequestCompleted(const URLRequest& request);

  // Notifies |this| that |request| has an expected response body size in octets
  // (8-bit bytes). |expected_content_size| is an estimate of total body length
  // based on the Content-Length header field when available or a general size
  // estimate when the Content-Length is not provided.
  void NotifyExpectedResponseContentSize(const URLRequest& request,
                                         int64_t expected_content_size);

  // Notifies |this| of a change in connection type.
  void OnConnectionTypeChanged();

  // |use_localhost_requests| should only be true when testing against local
  // HTTP server and allows the requests to local host to be used for network
  // quality estimation.
  void SetUseLocalHostRequestsForTesting(bool use_localhost_requests);

  // Returns true if throughput is currently tracked by a throughput
  // observation window.
  bool IsCurrentlyTrackingThroughput() const;

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Returns the number of bits received by Chromium so far. The count may not
  // start from zero, so the caller should only look at difference from a prior
  // call. The count is obtained by polling TrafficStats on Android, and
  // net::NetworkActivityMonitor on all other platforms. Virtualized for
  // testing.
  virtual int64_t GetBitsReceived() const;

  // Returns the number of in-flight requests that can be used for computing
  // throughput.
  size_t CountActiveInFlightRequests() const;

  // Returns the total number of in-flight requests. This also includes hanging
  // requests.
  size_t CountTotalInFlightRequests() const;

  // Returns the sum of expected response content size in bytes for all inflight
  // requests. Request with an unknown response content size have the default
  // response content size.
  int64_t CountTotalContentSizeBytes() const;

 protected:
  // Exposed for testing.
  bool disable_throughput_measurements_for_testing() const {
    return disable_throughput_measurements_;
  }

  // Removes hanging requests from |requests_|. If any hanging requests are
  // detected to be in-flight, the observation window is ended. Protected for
  // testing.
  void EraseHangingRequests(const URLRequest& request);

  // Returns true if the current throughput observation window is heuristically
  // determined to contain hanging requests.
  bool IsHangingWindow(int64_t bits_received,
                       base::TimeDelta duration,
                       double downstream_kbps_double) const;

 private:
  friend class TestThroughputAnalyzer;

  // Mapping from URL request to the expected content size of the response body
  // for that request. The map tracks all inflight requests. If the expected
  // content size is not available, the value is set to the default value.
  typedef std::unordered_map<const URLRequest*, int64_t> ResponseContentSizes;

  // Mapping from URL request to the last time data was received for that
  // request.
  typedef std::unordered_map<const URLRequest*, base::TimeTicks> Requests;

  // Set of URL requests to hold the requests that reduce the accuracy of
  // throughput computation. These requests are not used in throughput
  // computation.
  typedef std::unordered_set<const URLRequest*> AccuracyDegradingRequests;

  // Updates the response content size map for |request|. Also keeps the total
  // response content size counter updated. Adds an new entry if there is no
  // matching record in the map.
  void UpdateResponseContentSize(const URLRequest* request,
                                 int64_t response_size);

  // Returns true if downstream throughput can be recorded. In that case,
  // |downstream_kbps| is set to the computed downstream throughput (in
  // kilobits per second). If a downstream throughput observation is taken,
  // then the throughput observation window is reset so as to continue
  // tracking throughput. A throughput observation can be taken only if the
  // time-window is currently active, and enough bytes have accumulated in
  // that window. |downstream_kbps| should not be null.
  bool MaybeGetThroughputObservation(int32_t* downstream_kbps);

  // Starts the throughput observation window that keeps track of network
  // bytes if the following conditions are true:
  // (i) All active requests are non-local;
  // (ii) There is at least one active, non-local request; and,
  // (iii) The throughput observation window is not already tracking
  // throughput. The window is started by setting the |start_| and
  // |bits_received_|.
  void MaybeStartThroughputObservationWindow();

  // EndThroughputObservationWindow ends the throughput observation window.
  void EndThroughputObservationWindow();

  // Returns true if the |request| degrades the accuracy of the throughput
  // observation window. A local request or a request that spans a connection
  // change degrades the accuracy of the throughput computation.
  bool DegradesAccuracy(const URLRequest& request) const;

  // Bounds |accuracy_degrading_requests_| and |requests_| to ensure their sizes
  // do not exceed their capacities.
  void BoundRequestsSize();

  // Guaranteed to be non-null during the duration of |this|.
  const NetworkQualityEstimator* network_quality_estimator_;

  // Guaranteed to be non-null during the duration of |this|.
  const NetworkQualityEstimatorParams* params_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Called every time a new throughput observation is available.
  ThroughputObservationCallback throughput_observation_callback_;

  // Guaranteed to be non-null during the lifetime of |this|.
  const base::TickClock* tick_clock_;

  // Time when last connection change was observed.
  base::TimeTicks last_connection_change_;

  // Start time of the current throughput observation window. Set to null if
  // the window is not currently active.
  base::TimeTicks window_start_time_;

  // Number of bits received prior to |start_| as reported by
  // NetworkActivityMonitor.
  int64_t bits_received_at_window_start_;

  // Container that holds active requests that reduce the accuracy of
  // throughput computation. These requests are not used in throughput
  // computation.
  AccuracyDegradingRequests accuracy_degrading_requests_;

  // Container that holds active requests that do not reduce the accuracy of
  // throughput computation. These requests are used in throughput computation.
  Requests requests_;

  // Container that holds inflight request sizes. These requests are used in
  // computing the total of response content size for all inflight requests.
  ResponseContentSizes response_content_sizes_;

  // The running total of response content size for all inflight requests.
  int64_t total_response_content_size_;

  // Last time when the check for hanging requests was run.
  base::TimeTicks last_hanging_request_check_;

  // If true, then |this| throughput analyzer stops tracking the throughput
  // observations until Chromium is restarted. This may happen if the throughput
  // analyzer has lost track of the requests that degrade throughput computation
  // accuracy.
  bool disable_throughput_measurements_;

  // Determines if the requests to local host can be used in estimating the
  // network quality. Set to true only for tests.
  bool use_localhost_requests_for_tests_;

  SEQUENCE_CHECKER(sequence_checker_);

  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(ThroughputAnalyzer);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_THROUGHPUT_ANALYZER_H_
