// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/stateless_device.h"

#include <fcntl.h>
#include <linux/media.h>
#include <linux/videodev2.h>

#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
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

StatelessDevice::StatelessDevice() {}

StatelessDevice::~StatelessDevice() {}

bool StatelessDevice::Open() {
  if (!OpenDevice()) {
    return false;
  }

  return OpenMedia();
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

// VIDIOC_S_EXT_CTRLS
bool StatelessDevice::SetHeaders(void* ctrls,
                                 const base::ScopedFD& request_fd) {
  DVLOGF(4);
  if (!ctrls) {
    return false;
  }

  struct v4l2_ext_controls* ext_ctrls =
      static_cast<struct v4l2_ext_controls*>(ctrls);

  if (request_fd.is_valid()) {
    ext_ctrls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
  } else {
    ext_ctrls->which = V4L2_CTRL_WHICH_CUR_VAL;
  }

  ext_ctrls->request_fd = request_fd.get();

  return (IoctlDevice(VIDIOC_S_EXT_CTRLS, ext_ctrls) == kIoctlOk);
}

// MEDIA_IOC_REQUEST_ALLOC
base::ScopedFD StatelessDevice::CreateRequestFD() {
  DVLOGF(4);
  int request_fd;
  const int ret = Ioctl(media_fd_, MEDIA_IOC_REQUEST_ALLOC, &request_fd);
  if (ret != kIoctlOk) {
    VPLOGF(1) << "Failed to create request file descriptor.";
    return base::ScopedFD(-1);
  }

  return base::ScopedFD(request_fd);
}

// MEDIA_REQUEST_IOC_QUEUE
bool StatelessDevice::QueueRequest(const base::ScopedFD& request_fd) {
  DVLOGF(4);
  return (Ioctl(request_fd, MEDIA_REQUEST_IOC_QUEUE, nullptr) == kIoctlOk);
}

bool StatelessDevice::OpenMedia() {
  DVLOGF(4);

  struct v4l2_capability caps;
  if (IoctlDevice(VIDIOC_QUERYCAP, &caps) != kIoctlOk) {
    return false;
  }

  // Some devices, namely the RK3399, have multiple hardware decoder blocks.
  // We have to find and use the matching media device, or the kernel gets
  // confused.
  // Note that the match persists for the lifetime of V4L2Device. In practice
  // this should be fine, since |GetRequestsQueue()| is only called after
  // the codec format is configured, and the VD/VDA instance is always tied
  // to a specific format, so it will never need to switch media devices.
  static const std::string kRequestDevicePrefix = "/dev/media-dec";

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  base::ScopedFD media_fd;
  for (int i = 0; i < 10; ++i) {
    const auto path = kRequestDevicePrefix + base::NumberToString(i);
    media_fd_.reset(HANDLE_EINTR(open(path.c_str(), O_RDWR, 0)));
    if (!media_fd_.is_valid()) {
      VPLOGF(2) << "Failed to open media device: " << path;
      continue;
    }

    struct media_device_info media_info;
    if (Ioctl(media_fd_, MEDIA_IOC_DEVICE_INFO, &media_info) != kIoctlOk) {
      continue;
    }

    // Match the video driver and the media controller by the bus_info
    // field. This works better than the driver field if there are multiple
    // instances of the same decoder driver in the system. However old MediaTek
    // drivers didn't fill in the bus_info field for the media drivers.
    if (strlen(reinterpret_cast<const char*>(caps.bus_info)) > 0 &&
        strlen(reinterpret_cast<const char*>(media_info.bus_info)) > 0 &&
        strncmp(reinterpret_cast<const char*>(caps.bus_info),
                reinterpret_cast<const char*>(media_info.bus_info),
                sizeof(caps.bus_info))) {
      continue;
    }

    // Fall back to matching the video driver and the media controller by the
    // driver field. The mtk-vcodec driver does not fill the card and bus fields
    // properly, so those won't work.
    if (strncmp(reinterpret_cast<const char*>(caps.driver),
                reinterpret_cast<const char*>(media_info.driver),
                sizeof(caps.driver))) {
      continue;
    }

    break;
  }

  if (!media_fd_.is_valid()) {
    VLOGF(1) << "Failed to open request queue fd.";
    return false;
  }

  return true;
}

std::string StatelessDevice::DevicePath() {
  // TODO(frkoenig) : better querying?
  return "/dev/video-dec0";
}
}  //  namespace media
