// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/stateless_device.h"

#include <linux/videodev2.h>

#include "build/build_config.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_utils.h"

// TODO(frkoenig): put these into a common header
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME v4l2_fourcc('A', 'V', '1', 'F')
#endif

namespace media {

bool StatelessDevice::Open() {
  return OpenDevice();
}

bool StatelessDevice::CheckCapabilities(VideoCodec codec) {
  const auto supported_codecs = EnumerateInputFormats();
  if (!supported_codecs.contains(codec)) {
    DVLOGF(1) << "Driver does not support " << GetCodecName(codec);
    return false;
  }

  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (IoctlDevice(VIDIOC_QUERYCAP, &caps) != kIoctlOk) {
    return false;
  }

  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DVLOGF(1) << "Capabilities check failed: 0x" << std::hex
              << caps.capabilities;
    return false;
  }

  // The OUTPUT queue must support the request API for stateless decoding.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (IoctlDevice(VIDIOC_REQBUFS, &reqbufs) != kIoctlOk) {
    return false;
  }

  if ((reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_REQUESTS) !=
      V4L2_BUF_CAP_SUPPORTS_REQUESTS) {
    DVLOGF(1) << "OUTPUT queue does not support necessary request API. "
                 "Supported capabilities: 0x"
              << std::hex << reqbufs.capabilities;
    return false;
  }

  return true;
}

bool StatelessDevice::IsCompressedVP9HeaderSupported() {
  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = V4L2_CID_STATELESS_VP9_COMPRESSED_HDR;

  return (IoctlDevice(VIDIOC_QUERYCTRL, &query_ctrl) == kIoctlOk);
}

StatelessDevice::~StatelessDevice() {}

uint32_t StatelessDevice::VideoCodecToV4L2PixFmt(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return V4L2_PIX_FMT_H264_SLICE;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      return V4L2_PIX_FMT_HEVC_SLICE;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kVP8:
      return V4L2_PIX_FMT_VP8_FRAME;
    case VideoCodec::kVP9:
      return V4L2_PIX_FMT_VP9_FRAME;
    case VideoCodec::kAV1:
      return V4L2_PIX_FMT_AV1_FRAME;
    default:
      return 0;
  }
}

std::string StatelessDevice::DevicePath() {
  // TODO(frkoenig) : better querying?
  return "/dev/video-dec0";
}
}  //  namespace media
