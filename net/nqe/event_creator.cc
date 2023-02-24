// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/event_creator.h"

#include <stdlib.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"

namespace net::nqe::internal {

namespace {

base::Value::Dict NetworkQualityChangedNetLogParams(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps,
    EffectiveConnectionType effective_connection_type) {
  base::Value::Dict value;
  value.Set("http_rtt_ms", static_cast<int>(http_rtt.InMilliseconds()));
  value.Set("transport_rtt_ms",
            static_cast<int>(transport_rtt.InMilliseconds()));
  value.Set("downstream_throughput_kbps", downstream_throughput_kbps);
  value.Set("effective_connection_type",
            GetNameForEffectiveConnectionType(effective_connection_type));
  return value;
}

bool MetricChangedMeaningfully(int32_t past_value, int32_t current_value) {
  if ((past_value == INVALID_RTT_THROUGHPUT) !=
      (current_value == INVALID_RTT_THROUGHPUT)) {
    return true;
  }

  if (past_value == INVALID_RTT_THROUGHPUT &&
      current_value == INVALID_RTT_THROUGHPUT) {
    return false;
  }

  // Create a new entry only if (i) the difference between the two values exceed
  // the threshold; and, (ii) the ratio of the values also exceeds the
  // threshold.
  static const int kMinDifferenceInMetrics = 100;
  static const float kMinRatio = 1.2f;

  if (std::abs(past_value - current_value) < kMinDifferenceInMetrics) {
    // The absolute change in the value is not sufficient enough.
    return false;
  }

  if (past_value < (kMinRatio * current_value) &&
      current_value < (kMinRatio * past_value)) {
    // The relative change in the value is not sufficient enough.
    return false;
  }

  return true;
}

}  // namespace

EventCreator::EventCreator(NetLogWithSource net_log) : net_log_(net_log) {}

EventCreator::~EventCreator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EventCreator::MaybeAddNetworkQualityChangedEventToNetLog(
    EffectiveConnectionType effective_connection_type,
    const NetworkQuality& network_quality) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if any of the network quality metrics changed meaningfully.
  bool effective_connection_type_changed =
      past_effective_connection_type_ != effective_connection_type;
  bool http_rtt_changed = MetricChangedMeaningfully(
      past_network_quality_.http_rtt().InMilliseconds(),
      network_quality.http_rtt().InMilliseconds());

  bool transport_rtt_changed = MetricChangedMeaningfully(
      past_network_quality_.transport_rtt().InMilliseconds(),
      network_quality.transport_rtt().InMilliseconds());
  bool kbps_changed = MetricChangedMeaningfully(
      past_network_quality_.downstream_throughput_kbps(),
      network_quality.downstream_throughput_kbps());

  if (!effective_connection_type_changed && !http_rtt_changed &&
      !transport_rtt_changed && !kbps_changed) {
    // Return since none of the metrics changed meaningfully.
    return;
  }

  past_effective_connection_type_ = effective_connection_type;
  past_network_quality_ = network_quality;

  net_log_.AddEvent(NetLogEventType::NETWORK_QUALITY_CHANGED, [&] {
    return NetworkQualityChangedNetLogParams(
        network_quality.http_rtt(), network_quality.transport_rtt(),
        network_quality.downstream_throughput_kbps(),
        effective_connection_type);
  });
}

}  // namespace net::nqe::internal
