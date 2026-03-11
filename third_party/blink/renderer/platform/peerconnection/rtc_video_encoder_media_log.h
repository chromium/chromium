// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_MEDIA_LOG_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_MEDIA_LOG_H_

#include "media/base/media_log.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT RTCVideoEncoderMediaLog : public media::MediaLog {
 public:
  RTCVideoEncoderMediaLog();
  RTCVideoEncoderMediaLog(const RTCVideoEncoderMediaLog&) = delete;
  RTCVideoEncoderMediaLog& operator=(const RTCVideoEncoderMediaLog&) = delete;
  ~RTCVideoEncoderMediaLog() override;

  void AddLogRecordLocked(
      std::unique_ptr<media::MediaLogRecord> event) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_VIDEO_ENCODER_MEDIA_LOG_H_
