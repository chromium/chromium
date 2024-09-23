// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_types.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace media {

std::string VideoPixelFormatToString(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
      return "PIXEL_FORMAT_UNKNOWN";
    case PIXEL_FORMAT_I420:
      return "PIXEL_FORMAT_I420";
    case PIXEL_FORMAT_YV12:
      return "PIXEL_FORMAT_YV12";
    case PIXEL_FORMAT_I422:
      return "PIXEL_FORMAT_I422";
    case PIXEL_FORMAT_I420A:
      return "PIXEL_FORMAT_I420A";
    case PIXEL_FORMAT_I444:
      return "PIXEL_FORMAT_I444";
    case PIXEL_FORMAT_NV12:
      return "PIXEL_FORMAT_NV12";
    case PIXEL_FORMAT_NV21:
      return "PIXEL_FORMAT_NV21";
    case PIXEL_FORMAT_UYVY:
      return "PIXEL_FORMAT_UYVY";
    case PIXEL_FORMAT_YUY2:
      return "PIXEL_FORMAT_YUY2";
    case PIXEL_FORMAT_ARGB:
      return "PIXEL_FORMAT_ARGB";
    case PIXEL_FORMAT_XRGB:
      return "PIXEL_FORMAT_XRGB";
    case PIXEL_FORMAT_RGB24:
      return "PIXEL_FORMAT_RGB24";
    case PIXEL_FORMAT_MJPEG:
      return "PIXEL_FORMAT_MJPEG";
    case PIXEL_FORMAT_YUV420P9:
      return "PIXEL_FORMAT_YUV420P9";
    case PIXEL_FORMAT_YUV420P10:
      return "PIXEL_FORMAT_YUV420P10";
    case PIXEL_FORMAT_YUV422P9:
      return "PIXEL_FORMAT_YUV422P9";
    case PIXEL_FORMAT_YUV422P10:
      return "PIXEL_FORMAT_YUV422P10";
    case PIXEL_FORMAT_YUV444P9:
      return "PIXEL_FORMAT_YUV444P9";
    case PIXEL_FORMAT_YUV444P10:
      return "PIXEL_FORMAT_YUV444P10";
    case PIXEL_FORMAT_YUV420P12:
      return "PIXEL_FORMAT_YUV420P12";
    case PIXEL_FORMAT_YUV422P12:
      return "PIXEL_FORMAT_YUV422P12";
    case PIXEL_FORMAT_YUV444P12:
      return "PIXEL_FORMAT_YUV444P12";
    case PIXEL_FORMAT_Y16:
      return "PIXEL_FORMAT_Y16";
    case PIXEL_FORMAT_ABGR:
      return "PIXEL_FORMAT_ABGR";
    case PIXEL_FORMAT_XBGR:
      return "PIXEL_FORMAT_XBGR";
    case PIXEL_FORMAT_P010LE:
      return "PIXEL_FORMAT_P010LE";
    case PIXEL_FORMAT_XR30:
      return "PIXEL_FORMAT_XR30";
    case PIXEL_FORMAT_XB30:
      return "PIXEL_FORMAT_XB30";
    case PIXEL_FORMAT_BGRA:
      return "PIXEL_FORMAT_BGRA";
    case PIXEL_FORMAT_RGBAF16:
      return "PIXEL_FORMAT_RGBAF16";
    case PIXEL_FORMAT_I422A:
      return "PIXEL_FORMAT_I422A";
    case PIXEL_FORMAT_I444A:
      return "PIXEL_FORMAT_I444A";
    case PIXEL_FORMAT_YUV420AP10:
      return "PIXEL_FORMAT_YUV420AP10";
    case PIXEL_FORMAT_YUV422AP10:
      return "PIXEL_FORMAT_YUV422AP10";
    case PIXEL_FORMAT_YUV444AP10:
      return "PIXEL_FORMAT_YUV444AP10";
    case PIXEL_FORMAT_NV12A:
      return "PIXEL_FORMAT_NV12A";
    case PIXEL_FORMAT_NV16:
      return "PIXEL_FORMAT_NV16";
    case PIXEL_FORMAT_NV24:
      return "PIXEL_FORMAT_NV24";
    case PIXEL_FORMAT_P210LE:
      return "PIXEL_FORMAT_P210LE";
    case PIXEL_FORMAT_P410LE:
      return "PIXEL_FORMAT_P410LE";
  }
  NOTREACHED_IN_MIGRATION() << "Invalid VideoPixelFormat provided: " << format;
  return "";
}

std::string VideoChromaSamplingToString(VideoChromaSampling chroma_sampling) {
  switch (chroma_sampling) {
    case VideoChromaSampling::kUnknown:
      return "unknown chroma sampling";
    case VideoChromaSampling::k420:
      return "4:2:0";
    case VideoChromaSampling::k422:
      return "4:2:2";
    case VideoChromaSampling::k444:
      return "4:4:4";
    case VideoChromaSampling::k400:
      return "4:0:0";
  }
}

std::ostream& operator<<(std::ostream& os, VideoPixelFormat format) {
  os << VideoPixelFormatToString(format);
  return os;
}

std::string FourccToString(uint32_t fourcc) {
  std::string result = "0000";
  for (size_t i = 0; i < 4; ++i, fourcc >>= 8) {
    const char c = static_cast<char>(fourcc & 0xFF);
    if (c <= 0x1f || c >= 0x7f)
      return base::StringPrintf("0x%x", fourcc);
    result[i] = c;
  }
  return result;
}

VideoChromaSampling VideoPixelFormatToChromaSampling(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
      return VideoChromaSampling::kUnknown;
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_NV12A:
      return VideoChromaSampling::k420;
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_P210LE:
      return VideoChromaSampling::k422;
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P410LE:
      return VideoChromaSampling::k444;
    case PIXEL_FORMAT_Y16:
      return VideoChromaSampling::k400;
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_MJPEG:
      return VideoChromaSampling::kUnknown;
  }
  NOTREACHED() << "Invalid VideoPixelFormat provided: " << format;
}

bool IsYuvPlanar(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      return true;

    case PIXEL_FORMAT_UNKNOWN:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
      return false;
  }
  return false;
}

bool IsRGB(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
      return true;

    case PIXEL_FORMAT_UNKNOWN:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      return false;
  }
  return false;
}

bool IsOpaque(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
      return true;
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      break;
  }
  return false;
}

size_t BitDepth(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
      return 8;
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
      return 9;
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
      return 10;
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      return 12;
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_RGBAF16:
      return 16;
  }
  NOTREACHED();
}

}  // namespace media
