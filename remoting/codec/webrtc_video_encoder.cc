// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder.h"

#include "base/system/sys_info.h"

namespace remoting {

WebrtcVideoEncoder::EncodedFrame::EncodedFrame() = default;
WebrtcVideoEncoder::EncodedFrame::~EncodedFrame() = default;
WebrtcVideoEncoder::EncodedFrame::EncodedFrame(
    WebrtcVideoEncoder::EncodedFrame&&) = default;
WebrtcVideoEncoder::EncodedFrame& WebrtcVideoEncoder::EncodedFrame::operator=(
    WebrtcVideoEncoder::EncodedFrame&&) = default;

// static
int WebrtcVideoEncoder::GetEncoderThreadCount(int frame_width) {
  int thread_num;
  if (frame_width >= 5120) {
    thread_num = 32;
  } else if (frame_width >= 3840) {
    thread_num = 16;
  } else if (frame_width >= 2560) {
    thread_num = 8;
  } else if (frame_width >= 1280) {
    thread_num = 4;
  } else if (frame_width >= 720) {
    thread_num = 2;
  } else {
    thread_num = 1;
  }

  // Allow multiple cores on a system to be used for encoding to increase
  // performance while at the same time ensuring we don't bog down the system by
  // taking all of the available cores.
  return std::min(thread_num, ((base::SysInfo::NumberOfProcessors() + 1) / 2));
}

}  // namespace remoting
