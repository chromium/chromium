// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder.h"

#include <algorithm>

#include "base/system/sys_info.h"

namespace remoting {

namespace {

int GetEncoderThreadCountImpl(int frame_size, int num_processors) {
  int thread_num;
  if (frame_size >= 5120) {
    thread_num = 32;
  } else if (frame_size >= 3840) {
    thread_num = 16;
  } else if (frame_size >= 1920) {
    thread_num = 8;
  } else if (frame_size >= 1280) {
    thread_num = 4;
  } else if (frame_size >= 720) {
    thread_num = 2;
  } else {
    thread_num = 1;
  }

  // Allow multiple cores on a system to be used for encoding to increase
  // performance while at the same time ensuring we don't bog down the system by
  // taking all of the available cores.
  const int max_usable_threads = std::max(1, (num_processors + 1) / 2);
  return std::min(thread_num, max_usable_threads);
}

}  // namespace

WebrtcVideoEncoder::FrameStats::FrameStats() = default;
WebrtcVideoEncoder::FrameStats::FrameStats(const FrameStats&) = default;
WebrtcVideoEncoder::FrameStats& WebrtcVideoEncoder::FrameStats::operator=(
    const FrameStats&) = default;
WebrtcVideoEncoder::FrameStats::~FrameStats() = default;

std::unique_ptr<WebrtcVideoEncoder::FrameStats>
WebrtcVideoEncoder::FrameStats::Clone() const {
  return std::make_unique<FrameStats>(*this);
}

void WebrtcVideoEncoder::FrameStats::ResetTimestamps(base::TimeTicks now) {
  capture_started_time = now;
  capture_ended_time = now;
  encode_started_time = now;
}

WebrtcVideoEncoder::EncodedFrame::EncodedFrame() = default;
WebrtcVideoEncoder::EncodedFrame::~EncodedFrame() = default;
WebrtcVideoEncoder::EncodedFrame::EncodedFrame(
    WebrtcVideoEncoder::EncodedFrame&&) = default;
WebrtcVideoEncoder::EncodedFrame& WebrtcVideoEncoder::EncodedFrame::operator=(
    WebrtcVideoEncoder::EncodedFrame&&) = default;

// static
int WebrtcVideoEncoder::GetEncoderThreadCount(int frame_size) {
  return GetEncoderThreadCountImpl(frame_size,
                                   base::SysInfo::NumberOfProcessors());
}

// static
int WebrtcVideoEncoder::GetEncoderThreadCountForTesting(int frame_size,
                                                        int num_processors) {
  return GetEncoderThreadCountImpl(frame_size, num_processors);
}

}  // namespace remoting
