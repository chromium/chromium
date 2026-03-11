// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_media_log.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"

namespace blink {

RTCVideoEncoderMediaLog::RTCVideoEncoderMediaLog() = default;
RTCVideoEncoderMediaLog::~RTCVideoEncoderMediaLog() = default;

void RTCVideoEncoderMediaLog::AddLogRecordLocked(
    std::unique_ptr<media::MediaLogRecord> event) {
  if (event) {
    LOG(INFO) << "MediaLog: " << base::WriteJson(event->params).value_or("");
  }
}

}  // namespace blink
