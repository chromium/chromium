// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"

#include <sys/ioctl.h>

#include <unordered_map>

#include "base/containers/contains.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/video_types.h"

namespace media {

namespace v4l2_test {

constexpr int kIoctlOk = 0;

static const base::FilePath kDecodeDevice("/dev/video-dec0");

#define V4L2_REQUEST_CODE_AND_STRING(x) \
  { x, #x }

// This map maintains a table with pairs of V4L2 request code
// and corresponding name. New pair has to be added here
// when new V4L2 request code has to be used.
static const std::unordered_map<int, std::string>
    kMapFromV4L2RequestCodeToString = {
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_QUERYCAP),
        V4L2_REQUEST_CODE_AND_STRING(VIDIOC_ENUM_FMT)};

// Finds corresponding defined V4L2 request code name
// for a given V4L2 request code value.
std::string V4L2RequestCodeToString(int request_code) {
  DCHECK(base::Contains(kMapFromV4L2RequestCodeToString, request_code));

  const auto& request_code_pair =
      kMapFromV4L2RequestCodeToString.find(request_code);

  return request_code_pair->second;
}

V4L2IoctlShim::V4L2IoctlShim()
    : ioctl_fd_(kDecodeDevice,
                base::File::FLAG_OPEN | base::File::FLAG_READ |
                    base::File::FLAG_WRITE) {
  PCHECK(ioctl_fd_.IsValid()) << "Failed to open " << kDecodeDevice;
}

V4L2IoctlShim::~V4L2IoctlShim() = default;

template <typename T>
bool V4L2IoctlShim::Ioctl(int request_code, T* argp) const {
  NOTREACHED() << "Please add a specialized function for the given V4L2 ioctl "
                  "request code.";
  return !kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code, struct v4l2_capability* cap) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_QUERYCAP));
  LOG_ASSERT(cap != nullptr) << "|cap| check failed.\n";

  const int ret = ioctl(ioctl_fd_.GetPlatformFile(), request_code, cap);
  PLOG_IF(ERROR, ret != kIoctlOk) << "Ioctl request failed for VIDIOC_QUERYCAP";
  VLOG(4) << "Ioctl request was successful for VIDIOC_QUERYCAP.";

  return ret == kIoctlOk;
}

template <>
bool V4L2IoctlShim::Ioctl(int request_code,
                          struct v4l2_fmtdesc* fmtdesc) const {
  DCHECK_EQ(request_code, static_cast<int>(VIDIOC_ENUM_FMT));
  LOG_ASSERT(fmtdesc != nullptr) << "|fmtdesc| check failed.\n";

  const int ret = ioctl(ioctl_fd_.GetPlatformFile(), request_code, fmtdesc);
  PLOG_IF(ERROR, ret != kIoctlOk) << "Ioctl request failed for VIDIOC_ENUM_FMT";
  VLOG(4) << "Ioctl request was successful for VIDIOC_ENUM_FMT.";

  return ret == kIoctlOk;
}

bool V4L2IoctlShim::QueryFormat(enum v4l2_buf_type type,
                                uint32_t fourcc) const {
  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = type;

  while (Ioctl(VIDIOC_ENUM_FMT, &fmtdesc)) {
    if (fourcc == fmtdesc.pixelformat)
      return true;

    fmtdesc.index++;
  }

  return false;
}

bool V4L2IoctlShim::VerifyCapabilities(uint32_t compressed_format,
                                       uint32_t uncompressed_format) const {
  struct v4l2_capability cap;
  memset(&cap, 0, sizeof(cap));

  DCHECK(Ioctl(VIDIOC_QUERYCAP, &cap));

  LOG(INFO) << "Driver=\"" << cap.driver << "\" bus_info=\"" << cap.bus_info
            << "\" card=\"" << cap.card;

  const bool is_compressed_format_supported =
      QueryFormat(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, compressed_format);

  LOG_IF(ERROR, !is_compressed_format_supported)
      << media::FourccToString(compressed_format)
      << " is not a supported compressed OUTPUT format.";

  const bool is_uncompressed_format_supported =
      QueryFormat(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, uncompressed_format);

  LOG_IF(ERROR, !is_uncompressed_format_supported)
      << media::FourccToString(uncompressed_format)
      << " is not a supported uncompressed CAPTURE format.";

  return is_compressed_format_supported && is_uncompressed_format_supported;
}

}  // namespace v4l2_test
}  // namespace media