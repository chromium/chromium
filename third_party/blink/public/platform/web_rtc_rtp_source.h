// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_SOURCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_SOURCE_H_

#include <memory>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"

namespace base {
class TimeTicks;
}

namespace webrtc {
class RtpSource;
}

namespace blink {

// Represents both SSRCs and CSRCs.
// https://w3c.github.io/webrtc-pc/#dom-rtcrtpsynchronizationsource
// https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource
class BLINK_PLATFORM_EXPORT WebRTCRtpSource {
 public:
  enum class Type {
    kSSRC,
    kCSRC,
  };

  virtual ~WebRTCRtpSource();

  virtual Type SourceType() const = 0;
  virtual base::TimeTicks Timestamp() const = 0;
  virtual uint32_t Source() const = 0;
  virtual base::Optional<double> AudioLevel() const = 0;
  virtual uint32_t RtpTimestamp() const = 0;
};

BLINK_PLATFORM_EXPORT std::unique_ptr<WebRTCRtpSource> CreateRTCRtpSource(
    const webrtc::RtpSource& source);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_RTP_SOURCE_H_
