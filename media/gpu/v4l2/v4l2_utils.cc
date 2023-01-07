// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_utils.h"

#include <sstream>

#include "media/base/video_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

const char* V4L2MemoryToString(const v4l2_memory memory) {
  switch (memory) {
    case V4L2_MEMORY_MMAP:
      return "V4L2_MEMORY_MMAP";
    case V4L2_MEMORY_USERPTR:
      return "V4L2_MEMORY_USERPTR";
    case V4L2_MEMORY_DMABUF:
      return "V4L2_MEMORY_DMABUF";
    case V4L2_MEMORY_OVERLAY:
      return "V4L2_MEMORY_OVERLAY";
    default:
      return "UNKNOWN";
  }
}

std::string V4L2FormatToString(const struct v4l2_format& format) {
  std::ostringstream s;
  s << "v4l2_format type: " << format.type;
  if (format.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      format.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    const struct v4l2_pix_format& pix = format.fmt.pix;
    s << ", width_height: " << gfx::Size(pix.width, pix.height).ToString()
      << ", pixelformat: " << FourccToString(pix.pixelformat)
      << ", field: " << pix.field << ", bytesperline: " << pix.bytesperline
      << ", sizeimage: " << pix.sizeimage;
  } else if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    const struct v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
    // As long as num_planes's type is uint8_t, ostringstream treats it as a
    // char instead of an integer, which is not what we want. Casting
    // pix_mp.num_planes unsigned int solves the issue.
    s << ", width_height: " << gfx::Size(pix_mp.width, pix_mp.height).ToString()
      << ", pixelformat: " << FourccToString(pix_mp.pixelformat)
      << ", field: " << pix_mp.field
      << ", num_planes: " << static_cast<unsigned int>(pix_mp.num_planes);
    for (size_t i = 0; i < pix_mp.num_planes; ++i) {
      const struct v4l2_plane_pix_format& plane_fmt = pix_mp.plane_fmt[i];
      s << ", plane_fmt[" << i << "].sizeimage: " << plane_fmt.sizeimage
        << ", plane_fmt[" << i << "].bytesperline: " << plane_fmt.bytesperline;
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}

std::string V4L2BufferToString(const struct v4l2_buffer& buffer) {
  std::ostringstream s;
  s << "v4l2_buffer type: " << buffer.type << ", memory: " << buffer.memory
    << ", index: " << buffer.index << " bytesused: " << buffer.bytesused
    << ", length: " << buffer.length;
  if (buffer.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
      buffer.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
    //  single-planar
    if (buffer.memory == V4L2_MEMORY_MMAP) {
      s << ", m.offset: " << buffer.m.offset;
    } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
      s << ", m.userptr: " << buffer.m.userptr;
    } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
      s << ", m.fd: " << buffer.m.fd;
    }
  } else if (V4L2_TYPE_IS_MULTIPLANAR(buffer.type)) {
    for (size_t i = 0; i < buffer.length; ++i) {
      const struct v4l2_plane& plane = buffer.m.planes[i];
      s << ", m.planes[" << i << "](bytesused: " << plane.bytesused
        << ", length: " << plane.length
        << ", data_offset: " << plane.data_offset;
      if (buffer.memory == V4L2_MEMORY_MMAP) {
        s << ", m.mem_offset: " << plane.m.mem_offset;
      } else if (buffer.memory == V4L2_MEMORY_USERPTR) {
        s << ", m.userptr: " << plane.m.userptr;
      } else if (buffer.memory == V4L2_MEMORY_DMABUF) {
        s << ", m.fd: " << plane.m.fd;
      }
      s << ")";
    }
  } else {
    s << " unsupported yet.";
  }
  return s.str();
}
}  // namespace media
