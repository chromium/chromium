// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"

#include <cmath>

#include "base/time/time.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

namespace {

// Time between the NTP and Unix epochs.
//   unix_time = ntp_time - kNtpUnixEpochOffset
//   ntp_time = unix_time + kNtpUnixEpochOffset
constexpr base::TimeDelta kNtpUnixEpochOffset =
    base::Milliseconds(2208988800000);

Performance* GetPerformanceFromExecutionContext(ExecutionContext* context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    return DOMWindowPerformance::performance(*window);
  } else if (auto* worker = DynamicTo<WorkerGlobalScope>(context)) {
    return WorkerGlobalScopePerformance::performance(*worker);
  }
  NOTREACHED();
}

DOMHighResTimeStamp RTCEncodedFrameTimestampFromUnixRealClock(
    ExecutionContext* context,
    base::TimeDelta time_since_unix_epoch) {
  Performance* performance = GetPerformanceFromExecutionContext(context);
  return Performance::ClampTimeResolution(
      time_since_unix_epoch - base::Milliseconds(performance->timeOrigin()),
      performance->CrossOriginIsolatedCapability());
}

}  // namespace

DOMHighResTimeStamp RTCTimeStampFromTimeTicks(ExecutionContext* context,
                                              base::TimeTicks timestamp) {
  Performance* performance = GetPerformanceFromExecutionContext(context);
  return Performance::MonotonicTimeToDOMHighResTimeStamp(
      performance->GetTimeOriginInternal(), timestamp,
      /*allow_negative_value=*/true,
      performance->CrossOriginIsolatedCapability());
}

DOMHighResTimeStamp RTCEncodedFrameTimestampFromCaptureTimeInfo(
    ExecutionContext* context,
    CaptureTimeInfo capture_time_info) {
  switch (capture_time_info.clock_type) {
    case CaptureTimeInfo::ClockType::kTimeTicks:
      return RTCTimeStampFromTimeTicks(
          context, base::TimeTicks() + capture_time_info.capture_time);
    case CaptureTimeInfo::ClockType::kNtpRealClock:
      base::TimeDelta time_since_unix_epoch =
          capture_time_info.capture_time - kNtpUnixEpochOffset;
      return RTCEncodedFrameTimestampFromUnixRealClock(context,
                                                       time_since_unix_epoch);
  }
}

base::TimeDelta RTCEncodedFrameTimestampToCaptureTime(
    ExecutionContext* context,
    DOMHighResTimeStamp timestamp,
    CaptureTimeInfo::ClockType clock_type) {
  Performance* performance = GetPerformanceFromExecutionContext(context);
  switch (clock_type) {
    case CaptureTimeInfo::ClockType::kTimeTicks:
      return (performance->GetTimeOriginInternal() +
              base::Milliseconds(timestamp))
          .since_origin();
    case CaptureTimeInfo::ClockType::kNtpRealClock:
      return kNtpUnixEpochOffset +
             base::Milliseconds(performance->timeOrigin() + timestamp);
  }
}

DOMHighResTimeStamp CalculateRTCEncodedFrameTimeDelta(
    ExecutionContext* context,
    base::TimeDelta time_delta) {
  return Performance::ClampTimeResolution(
      time_delta, GetPerformanceFromExecutionContext(context)
                      ->CrossOriginIsolatedCapability());
}

double ToLinearAudioLevel(uint8_t audio_level_dbov) {
  if (audio_level_dbov >= 127u) {
    return 0.0;
  }
  return std::pow(10.0, -static_cast<double>(audio_level_dbov) / 20.0);
}

uint8_t FromLinearAudioLevel(double linear_audio_level) {
  if (linear_audio_level <= 0.0) {
    return 127u;
  }
  double audio_level_dbov = -20.0 * std::log10(linear_audio_level);
  if (audio_level_dbov >= 127.0) {
    return 127u;
  }
  if (audio_level_dbov < 0.0) {
    return 0u;
  }
  return static_cast<uint8_t>(std::round(audio_level_dbov));
}

}  // namespace blink
