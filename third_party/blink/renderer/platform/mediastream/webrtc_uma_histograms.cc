// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"

namespace blink {

void LogUserMediaRequestResult(mojom::blink::MediaStreamRequestResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "WebRTC.UserMediaRequest.Result2", result,
      mojom::blink::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS);
}

void UpdateWebRTCMethodCount(RTCAPIName api_name) {
  DVLOG(3) << "Incrementing WebRTC.webkitApiCount for "
           << static_cast<int>(api_name);
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCount", api_name,
                            RTCAPIName::kInvalidName);
  PerSessionWebRTCAPIMetrics::GetInstance()->LogUsageOnlyOnce(api_name);
}

PerSessionWebRTCAPIMetrics::~PerSessionWebRTCAPIMetrics() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
PerSessionWebRTCAPIMetrics* PerSessionWebRTCAPIMetrics::GetInstance() {
  return base::Singleton<PerSessionWebRTCAPIMetrics>::get();
}

void PerSessionWebRTCAPIMetrics::IncrementStreamCounter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_streams_;
}

void PerSessionWebRTCAPIMetrics::DecrementStreamCounter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (--num_streams_ == 0) {
    ResetUsage();
  }
}

PerSessionWebRTCAPIMetrics::PerSessionWebRTCAPIMetrics() : num_streams_(0) {
  ResetUsage();
}

void PerSessionWebRTCAPIMetrics::LogUsage(RTCAPIName api_name) {
  DVLOG(3) << "Incrementing WebRTC.webkitApiCountPerSession for "
           << static_cast<int>(api_name);
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCountPerSession", api_name,
                            RTCAPIName::kInvalidName);
}

void PerSessionWebRTCAPIMetrics::LogUsageOnlyOnce(RTCAPIName api_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!has_used_api_[static_cast<int>(api_name)]) {
    has_used_api_[static_cast<int>(api_name)] = true;
    LogUsage(api_name);
  }
}

void PerSessionWebRTCAPIMetrics::ResetUsage() {
  for (bool& has_used_api : has_used_api_)
    has_used_api = false;
}

}  // namespace blink
