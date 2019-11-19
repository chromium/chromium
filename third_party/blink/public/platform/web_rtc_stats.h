// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace webrtc {
class RTCStatsCollectorCallback;
enum class NonStandardGroupId;
}  // namespace webrtc

namespace blink {

class RTCStatsReportPlatform;

using WebRTCStatsReportCallback =
    base::OnceCallback<void(std::unique_ptr<RTCStatsReportPlatform>)>;

BLINK_PLATFORM_EXPORT
rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback>
CreateRTCStatsCollectorCallback(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    WebRTCStatsReportCallback callback,
    const WebVector<webrtc::NonStandardGroupId>& exposed_group_ids);

BLINK_PLATFORM_EXPORT void WhitelistStatsForTesting(const char* type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_STATS_H_
