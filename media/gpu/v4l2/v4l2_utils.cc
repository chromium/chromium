// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_utils.h"

#include <fcntl.h>
#include <sys/ioctl.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#endif

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME v4l2_fourcc('A', 'V', '1', 'F')
#endif

#define MAKE_V4L2_CODEC_PAIR(codec, suffix) \
  std::make_pair(codec##_##suffix, codec)

namespace {
int HandledIoctl(int fd, int request, void* arg) {
  return HANDLE_EINTR(ioctl(fd, request, arg));
}

std::string GetDriverName(const media::IoctlAsCallback& ioctl_cb) {
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  if (ioctl_cb.Run(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP" << ", caps check failed: 0x"
              << std::hex << caps.capabilities;
    return "";
  }

  return std::string(reinterpret_cast<const char*>(caps.driver));
}
}  // namespace
namespace media {

void RecordMediaIoctlUMA(MediaIoctlRequests function) {
  base::UmaHistogramEnumeration("Media.V4l2VideoDecoder.MediaIoctlError",
                                function);
}

void RecordVidiocIoctlErrorUMA(VidiocIoctlRequests function) {
  base::UmaHistogramEnumeration("Media.V4l2VideoDecoder.VidiocIoctlError",
                                function);
}

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

VideoCodecProfile V4L2ProfileToVideoCodecProfile(uint32_t v4l2_codec,
                                                 uint32_t v4l2_profile) {
  switch (v4l2_codec) {
    case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
      switch (v4l2_profile) {
        // H264 Stereo amd Multiview High are not tested and the use is
        // minuscule, skip.
        case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
        case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
          return H264PROFILE_BASELINE;
        case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
          return H264PROFILE_MAIN;
        case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
          return H264PROFILE_EXTENDED;
        case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
          return H264PROFILE_HIGH;
      }
      break;
    case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_VP8_PROFILE_0:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_1:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_2:
        case V4L2_MPEG_VIDEO_VP8_PROFILE_3:
          return VP8PROFILE_ANY;
      }
      break;
    case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
      switch (v4l2_profile) {
        // VP9 Profile 1 and 3 are not tested and the use is minuscule, skip.
        case V4L2_MPEG_VIDEO_VP9_PROFILE_0:
          return VP9PROFILE_PROFILE0;
        case V4L2_MPEG_VIDEO_VP9_PROFILE_2:
          return VP9PROFILE_PROFILE2;
      }
      break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN:
          return HEVCPROFILE_MAIN;
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE:
          return HEVCPROFILE_MAIN_STILL_PICTURE;
        case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10:
          return HEVCPROFILE_MAIN10;
      }
      break;
#endif
#if BUILDFLAG(IS_CHROMEOS)
    case V4L2_CID_MPEG_VIDEO_AV1_PROFILE:
      switch (v4l2_profile) {
        case V4L2_MPEG_VIDEO_AV1_PROFILE_MAIN:
          return AV1PROFILE_PROFILE_MAIN;
        case V4L2_MPEG_VIDEO_AV1_PROFILE_HIGH:
          return AV1PROFILE_PROFILE_HIGH;
        case V4L2_MPEG_VIDEO_AV1_PROFILE_PROFESSIONAL:
          return AV1PROFILE_PROFILE_PRO;
      }
      break;
#endif
  }
  return VIDEO_CODEC_PROFILE_UNKNOWN;
}

size_t GetNumPlanesOfV4L2PixFmt(uint32_t pix_fmt) {
  std::optional<Fourcc> fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (fourcc && fourcc->IsMultiPlanar()) {
    return VideoFrame::NumPlanes(fourcc->ToVideoPixelFormat());
  }
  return 1u;
}

std::optional<VideoFrameLayout> V4L2FormatToVideoFrameLayout(
    const struct v4l2_format& format) {
  if (!V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    VLOGF(1) << "v4l2_buf_type is not multiplanar: " << std::hex << "0x"
             << format.type;
    return std::nullopt;
  }
  const v4l2_pix_format_mplane& pix_mp = format.fmt.pix_mp;
  const uint32_t& pix_fmt = pix_mp.pixelformat;
  const auto video_fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
  if (!video_fourcc) {
    VLOGF(1) << "Failed to convert pixel format to VideoPixelFormat: "
             << FourccToString(pix_fmt);
    return std::nullopt;
  }
  const VideoPixelFormat video_format = video_fourcc->ToVideoPixelFormat();
  const size_t num_buffers = pix_mp.num_planes;
  const size_t num_color_planes = VideoFrame::NumPlanes(video_format);
  if (num_color_planes == 0) {
    VLOGF(1) << "Unsupported video format for NumPlanes(): "
             << VideoPixelFormatToString(video_format);
    return std::nullopt;
  }
  if (num_buffers > num_color_planes) {
    VLOGF(1) << "pix_mp.num_planes: " << num_buffers
             << " should not be larger than NumPlanes("
             << VideoPixelFormatToString(video_format)
             << "): " << num_color_planes;
    return std::nullopt;
  }
  // Reserve capacity in advance to prevent unnecessary vector reallocation.
  std::vector<ColorPlaneLayout> planes;
  planes.reserve(num_color_planes);
  for (size_t i = 0; i < num_buffers; ++i) {
    const v4l2_plane_pix_format& plane_format = pix_mp.plane_fmt[i];
    planes.emplace_back(static_cast<int32_t>(plane_format.bytesperline), 0u,
                        plane_format.sizeimage);
  }
  // For the case that #color planes > #buffers, it fills stride of color
  // plane which does not map to buffer.
  // Right now only some pixel formats are supported: NV12, YUV420, YVU420.
  if (num_color_planes > num_buffers) {
    const int32_t y_stride = planes[0].stride;
    // Note that y_stride is from v4l2 bytesperline and its type is uint32_t.
    // It is safe to cast to size_t.
    const size_t y_stride_abs = static_cast<size_t>(y_stride);
    switch (pix_fmt) {
      case V4L2_PIX_FMT_NV12:
        // The stride of UV is the same as Y in NV12.
        // The height is half of Y plane.
        planes.emplace_back(y_stride, y_stride_abs * pix_mp.height,
                            y_stride_abs * pix_mp.height / 2);
        DCHECK_EQ(2u, planes.size());
        break;
      case V4L2_PIX_FMT_YUV420:
      case V4L2_PIX_FMT_YVU420: {
        // The spec claims that two Cx rows (including padding) is exactly as
        // long as one Y row (including padding). So stride of Y must be even
        // number.
        if (y_stride % 2 != 0 || pix_mp.height % 2 != 0) {
          VLOGF(1) << "Plane-Y stride and height should be even; stride: "
                   << y_stride << ", height: " << pix_mp.height;
          return std::nullopt;
        }
        const int32_t half_stride = y_stride / 2;
        const size_t plane_0_area = y_stride_abs * pix_mp.height;
        const size_t plane_1_area = plane_0_area / 4;
        planes.emplace_back(half_stride, plane_0_area, plane_1_area);
        planes.emplace_back(half_stride, plane_0_area + plane_1_area,
                            plane_1_area);
        DCHECK_EQ(3u, planes.size());
        break;
      }
      default:
        VLOGF(1) << "Cannot derive stride for each plane for pixel format "
                 << FourccToString(pix_fmt);
        return std::nullopt;
    }
  }

  // Some V4L2 devices expect buffers to be page-aligned. We cannot detect
  // such devices individually, so set this as a video frame layout property.
  constexpr size_t buffer_alignment = 0x1000;
  if (num_buffers == 1) {
    return VideoFrameLayout::CreateWithPlanes(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  } else {
    return VideoFrameLayout::CreateMultiPlanar(
        video_format, gfx::Size(pix_mp.width, pix_mp.height), std::move(planes),
        buffer_alignment);
  }
}

namespace {
using v4l2_enum_type = decltype(V4L2_PIX_FMT_H264);
// Correspondence from V4L2 codec described as a pixel format to a Control ID.
static const std::map<v4l2_enum_type, v4l2_enum_type>
    kV4L2CodecPixFmtToProfileCID = {
        {V4L2_PIX_FMT_H264, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
        {V4L2_PIX_FMT_H264_SLICE, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_HEVC, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
        {V4L2_PIX_FMT_HEVC_SLICE, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_VP8, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP8_FRAME, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP9, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
        {V4L2_PIX_FMT_VP9_FRAME, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_PIX_FMT_AV1, V4L2_CID_MPEG_VIDEO_AV1_PROFILE},
        {V4L2_PIX_FMT_AV1_FRAME, V4L2_CID_MPEG_VIDEO_AV1_PROFILE},
#endif
};

// Default VideoCodecProfiles associated to a V4L2 Codec Control ID.
static const std::map<v4l2_enum_type, std::vector<VideoCodecProfile>>
    kDefaultVideoCodecProfilesForProfileCID = {
        {V4L2_CID_MPEG_VIDEO_H264_PROFILE,
         {
             H264PROFILE_BASELINE,
             H264PROFILE_MAIN,
             H264PROFILE_HIGH,
         }},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
         {HEVCPROFILE_MAIN, HEVCPROFILE_MAIN10}},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_VP8_PROFILE, {VP8PROFILE_ANY}},
        {V4L2_CID_MPEG_VIDEO_VP9_PROFILE, {VP9PROFILE_PROFILE0}},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_CID_MPEG_VIDEO_AV1_PROFILE, {AV1PROFILE_PROFILE_MAIN}},
#endif
};

// Correspondence from a VideoCodecProfiles to V4L2 codec described
// as a pixel format.
static const std::map<VideoCodecProfile,
                      std::pair<v4l2_enum_type, v4l2_enum_type>>
    kVideoCodecProfileToV4L2CodecPixFmt = {
        {H264PROFILE_BASELINE, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
        {H264PROFILE_MAIN, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
        {H264PROFILE_HIGH, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_H264, SLICE)},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {HEVCPROFILE_MAIN, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_HEVC, SLICE)},
        {HEVCPROFILE_MAIN10, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_HEVC, SLICE)},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {VP8PROFILE_ANY, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_VP8, FRAME)},
        {VP9PROFILE_PROFILE0, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_VP9, FRAME)},
        {VP9PROFILE_PROFILE2, MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_VP9, FRAME)},
#if BUILDFLAG(IS_CHROMEOS)
        {AV1PROFILE_PROFILE_MAIN,
         MAKE_V4L2_CODEC_PAIR(V4L2_PIX_FMT_AV1, FRAME)},
#endif
};

}  // namespace

std::vector<SVCScalabilityMode> GetSupportedScalabilityModesForV4L2Codec(
    const IoctlAsCallback& ioctl_cb,
    VideoCodecProfile media_profile) {
  std::vector<SVCScalabilityMode> scalability_modes;
  scalability_modes.push_back(SVCScalabilityMode::kL1T1);

  if (base::FeatureList::IsEnabled(kV4L2H264TemporalLayerHWEncoding) &&
      media_profile >= H264PROFILE_MIN && media_profile <= H264PROFILE_MAX) {
    struct v4l2_queryctrl query_ctrl;
    memset(&query_ctrl, 0, sizeof(query_ctrl));
    query_ctrl.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING;
    if (ioctl_cb.Run(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
      DPLOG(WARNING) << "h.264 hierarchical coding not supported.";
      return {};
    }

    memset(&query_ctrl, 0, sizeof(query_ctrl));
    query_ctrl.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE;
    if (ioctl_cb.Run(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
      DPLOG(WARNING) << "h.264 hierarchical coding type not supported.";
      return {};
    }

    struct v4l2_querymenu query_menu = {
        .id = query_ctrl.id, .index = static_cast<__u32>(query_ctrl.minimum)};
    for (; static_cast<int>(query_menu.index) <= query_ctrl.maximum;
         query_menu.index++) {
      if (ioctl_cb.Run(VIDIOC_QUERYMENU, &query_menu) != kIoctlOk) {
        continue;
      }

      if (query_menu.index == V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P) {
        break;
      }
    }

    if (query_menu.index != V4L2_MPEG_VIDEO_H264_HIERARCHICAL_CODING_P) {
      DPLOG(WARNING) << "h.264 hierarchical P coding not supported.";
      return {};
    }

    memset(&query_ctrl, 0, sizeof(query_ctrl));
    query_ctrl.id = V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER;
    if (ioctl_cb.Run(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
      DPLOG(WARNING) << "Unable to determine the number of layers supported.";
      return {};
    }

    if (query_ctrl.maximum >= 2) {
      DVLOGF(2) << "h.264 kL1T2 scalability mode supported.";
      scalability_modes.push_back(SVCScalabilityMode::kL1T2);
    }
  }

  return scalability_modes;
}

std::vector<VideoCodecProfile> EnumerateSupportedProfilesForV4L2Codec(
    const IoctlAsCallback& ioctl_cb,
    uint32_t codec_as_pix_fmt) {
  if (!base::Contains(kV4L2CodecPixFmtToProfileCID, codec_as_pix_fmt)) {
    // This is OK: there are many codecs that are not supported by Chrome.
    VLOGF(4) << "Unsupported codec: " << FourccToString(codec_as_pix_fmt);
    return {};
  }
  const auto profile_cid = kV4L2CodecPixFmtToProfileCID.at(codec_as_pix_fmt);

  v4l2_queryctrl query_ctrl = {.id = static_cast<__u32>(profile_cid)};
  if (ioctl_cb.Run(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
    // This happens for example for VP8 on Hana MTK8173, or for HEVC on Trogdor
    // QC SC7180) at the time of writing.
    DVLOGF(4) << "Driver doesn't support enumerating "
              << FourccToString(codec_as_pix_fmt)
              << " profiles, using default ones.";
    DCHECK(
        base::Contains(kDefaultVideoCodecProfilesForProfileCID, profile_cid));
    return kDefaultVideoCodecProfilesForProfileCID.at(profile_cid);
  }

  std::vector<VideoCodecProfile> profiles;

  v4l2_querymenu query_menu = {.id = query_ctrl.id,
                               .index = static_cast<__u32>(query_ctrl.minimum)};
  for (; static_cast<int>(query_menu.index) <= query_ctrl.maximum;
       query_menu.index++) {
    if (ioctl_cb.Run(VIDIOC_QUERYMENU, &query_menu) != kIoctlOk) {
      continue;
    }
    const VideoCodecProfile profile =
        V4L2ProfileToVideoCodecProfile(profile_cid, query_menu.index);
    DVLOGF_IF(4, profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        << "Profile: " << query_menu.name
        << " not supported by Chrome, skipping.";

    if (profile != VIDEO_CODEC_PROFILE_UNKNOWN) {
      profiles.push_back(profile);
      DVLOGF(4) << "Found supported profile: " << query_menu.name;
    }
  }

  // Erase duplicated profiles. This is needed because H264PROFILE_BASELINE maps
  // to both V4L2_MPEG_VIDEO_H264_PROFILE__BASELINE/CONSTRAINED_BASELINE
  base::ranges::sort(profiles);
  profiles.erase(base::ranges::unique(profiles), profiles.end());
  return profiles;
}

std::vector<uint32_t> EnumerateSupportedPixFmts(const IoctlAsCallback& ioctl_cb,
                                                v4l2_buf_type buf_type) {
  DCHECK(buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
         buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  std::vector<v4l2_enum_type> pix_fmts;
  v4l2_fmtdesc fmtdesc = {.type = buf_type};
  for (; ioctl_cb.Run(VIDIOC_ENUM_FMT, &fmtdesc) == kIoctlOk; ++fmtdesc.index) {
    DVLOGF(4) << "Enumerated "
              << (buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
                      ? "codec: "
                      : "pixel format: ")
              << FourccToString(fmtdesc.pixelformat) << " ("
              << fmtdesc.description << ")";
    pix_fmts.push_back(fmtdesc.pixelformat);
  }

  return pix_fmts;
}

void GetSupportedResolution(const IoctlAsCallback& ioctl_cb,
                            uint32_t pixelformat,
                            gfx::Size* min_resolution,
                            gfx::Size* max_resolution) {
  constexpr gfx::Size kDefaultMaxCodedSize(1920, 1088);
  *max_resolution = kDefaultMaxCodedSize;
  constexpr gfx::Size kDefaultMinCodedSize(16, 16);
  *min_resolution = kDefaultMinCodedSize;

  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = pixelformat;
  if (ioctl_cb.Run(VIDIOC_ENUM_FRAMESIZES, &frame_size) == kIoctlOk) {
    if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
      max_resolution->SetSize(frame_size.stepwise.max_width,
                              frame_size.stepwise.max_height);
      min_resolution->SetSize(frame_size.stepwise.min_width,
                              frame_size.stepwise.min_height);
    } else {
#if BUILDFLAG(IS_CHROMEOS)
      // All of Chrome-supported implementations support STEPWISE only.
      CHECK_EQ(frame_size.type, V4L2_FRMSIZE_TYPE_STEPWISE);
#endif
    }
  } else {
    DLOGF(INFO) << "VIDIOC_ENUM_FRAMESIZES failed, using default values";
  }
}

uint32_t VideoCodecProfileToV4L2PixFmt(VideoCodecProfile profile,
                                       bool slice_based) {
  CHECK(base::Contains(kVideoCodecProfileToV4L2CodecPixFmt, profile))
      << "Unsupported profile: " << GetProfileName(profile);

  const auto& v4l2_pix_fmt = kVideoCodecProfileToV4L2CodecPixFmt.at(profile);
  return slice_based ? v4l2_pix_fmt.first : v4l2_pix_fmt.second;
}

base::TimeDelta TimeValToTimeDelta(const struct timeval& timeval) {
  struct timespec ts;
  const struct timeval temp_timeval = timeval;
  TIMEVAL_TO_TIMESPEC(&temp_timeval, &ts);
  return base::TimeDelta::FromTimeSpec(ts);
}

struct timeval TimeDeltaToTimeVal(base::TimeDelta time_delta) {
  const int64_t time_delta_linear = time_delta.InMicroseconds();
  constexpr int64_t kMicrosecondsPerSecond = 1000 * 1000;
  return {.tv_sec = base::checked_cast<__time_t>(time_delta_linear /
                                                 kMicrosecondsPerSecond),
          .tv_usec = base::checked_cast<__suseconds_t>(time_delta_linear %
                                                       kMicrosecondsPerSecond)};
}

std::optional<SupportedVideoDecoderConfigs> GetSupportedV4L2DecoderConfigs() {
  SupportedVideoDecoderConfigs supported_media_configs;

  constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
  base::ScopedFD device_fd(HANDLE_EINTR(
      open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd.is_valid()) {
    PLOG(ERROR) << "Could not open " << kVideoDeviceDriverPath;
    return std::nullopt;
  }

  std::vector<uint32_t> v4l2_codecs = EnumerateSupportedPixFmts(
      base::BindRepeating(&HandledIoctl, device_fd.get()),
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  for (const uint32_t v4l2_codec : v4l2_codecs) {
    const std::vector<VideoCodecProfile> media_codec_profiles =
        EnumerateSupportedProfilesForV4L2Codec(
            base::BindRepeating(&HandledIoctl, device_fd.get()), v4l2_codec);

    gfx::Size min_coded_size;
    gfx::Size max_coded_size;
    GetSupportedResolution(base::BindRepeating(&HandledIoctl, device_fd.get()),
                           v4l2_codec, &min_coded_size, &max_coded_size);

    for (const auto& profile : media_codec_profiles) {
      supported_media_configs.emplace_back(SupportedVideoDecoderConfig(
          profile, profile, min_coded_size, max_coded_size,
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
          /*allow_encrypted=*/true,
#else
          /*allow_encrypted=*/false,
#endif
          /*require_encrypted=*/false));
    }
  }

#if DCHECK_IS_ON()
  for (const auto& config : supported_media_configs) {
    DVLOGF(3) << "Enumerated " << GetProfileName(config.profile_min) << " ("
              << config.coded_size_min.ToString() << "-"
              << config.coded_size_max.ToString() << ")";
  }
#endif

  return supported_media_configs;
}

bool IsV4L2DecoderStateful() {
  constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
  base::ScopedFD device_fd(HANDLE_EINTR(
      open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd.is_valid()) {
    return false;
  }

  std::vector<uint32_t> v4l2_codecs = EnumerateSupportedPixFmts(
      base::BindRepeating(&HandledIoctl, device_fd.get()),
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  // V4L2 stateful formats (don't end up with _SLICE or _FRAME) supported.
  constexpr std::array<uint32_t, 4> kSupportedStatefulInputCodecs = {
      V4L2_PIX_FMT_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      V4L2_PIX_FMT_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      V4L2_PIX_FMT_VP8,
      V4L2_PIX_FMT_VP9,
  };

  return std::find_first_of(v4l2_codecs.begin(), v4l2_codecs.end(),
                            kSupportedStatefulInputCodecs.begin(),
                            kSupportedStatefulInputCodecs.end()) !=
         v4l2_codecs.end();
}

bool IsVislDriver() {
  constexpr char kVideoDeviceDriverPath[] = "/dev/video-dec0";
  base::ScopedFD device_fd(HANDLE_EINTR(
      open(kVideoDeviceDriverPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  if (!device_fd.is_valid()) {
    return false;
  }

  std::string v4l2_driver_name =
      GetDriverName(base::BindRepeating(&HandledIoctl, device_fd.get()));

  return v4l2_driver_name.compare("visl") == 0;
}

#ifndef NDEBUG
template <class T>
std::string ControlToString(const T& a, const std::string control) {
  std::ostringstream s;
  constexpr uint32_t kPadding = 40;
  const uint32_t len = kPadding - control.size();
  s << control << std::setw(len) << ": ";
  if (control.compare("flags") == 0) {
    s << "0x" << std::hex << +a << std::endl;
  } else {
    s << std::dec << +a << std::endl;
  }

  return s.str();
}

template <typename T, size_t N>
std::string ControlToString(const T (&arr)[N], const std::string control) {
  std::ostringstream s;
  const uint32_t pad = sizeof(arr[0]) * 2;
  s << control << "[" << N << "]:" << std::endl;
  for (const auto& x : arr) {
    s << std::setw(pad) << std::dec << +x << " ";
  }
  s << std::endl;
  return s.str();
}

template <typename T, size_t N, size_t M>
std::string ControlToString(const T (&arr)[N][M], const std::string control) {
  std::ostringstream s;
  const uint32_t pad = sizeof(arr[0][0]) * 2;
  s << control << "[" << N << "][" << M << "]:" << std::endl;
  for (const auto& inner : arr) {
    for (const auto& x : inner) {
      s << std::setw(pad) << std::dec << +x;
    }
    s << std::endl;
  }

  return s.str();
}

#define CONTROL(base, control)                     \
  do {                                             \
    s << ControlToString(base->control, #control); \
  } while (0)

static std::string PrintStatelessAV1Control(
    const struct v4l2_ext_control* ext_ctrls) {
  std::ostringstream s;
  s << "av1" << std::endl;
  switch (ext_ctrls->id) {
    case V4L2_CID_STATELESS_AV1_SEQUENCE: {
      s << "V4L2_CID_STATELESS_AV1_SEQUENCE" << std::endl;
      const struct v4l2_ctrl_av1_sequence* ctrl =
          static_cast<const struct v4l2_ctrl_av1_sequence*>(ext_ctrls->ptr);
      CONTROL(ctrl, flags);
      CONTROL(ctrl, seq_profile);
      CONTROL(ctrl, order_hint_bits);
      CONTROL(ctrl, bit_depth);
      CONTROL(ctrl, reserved);
      CONTROL(ctrl, max_frame_width_minus_1);
      CONTROL(ctrl, max_frame_height_minus_1);
    } break;
    case V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY: {
      s << "V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY" << std::endl;
      const struct v4l2_ctrl_av1_tile_group_entry* group_entry =
          static_cast<const struct v4l2_ctrl_av1_tile_group_entry*>(
              ext_ctrls->ptr);
      CONTROL(group_entry, tile_offset);
      CONTROL(group_entry, tile_size);
      CONTROL(group_entry, tile_row);
      CONTROL(group_entry, tile_col);
    } break;
    case V4L2_CID_STATELESS_AV1_FRAME: {
      s << "V4L2_CID_STATELESS_AV1_FRAME" << std::endl;
      const struct v4l2_ctrl_av1_frame* frame =
          static_cast<const struct v4l2_ctrl_av1_frame*>(ext_ctrls->ptr);

      const struct v4l2_av1_tile_info* tile_info =
          static_cast<const struct v4l2_av1_tile_info*>(&frame->tile_info);
      CONTROL(tile_info, flags);
      CONTROL(tile_info, context_update_tile_id);
      CONTROL(tile_info, tile_cols);
      CONTROL(tile_info, tile_rows);
      CONTROL(tile_info, mi_col_starts);
      CONTROL(tile_info, mi_row_starts);
      CONTROL(tile_info, width_in_sbs_minus_1);
      CONTROL(tile_info, height_in_sbs_minus_1);
      CONTROL(tile_info, tile_size_bytes);
      CONTROL(tile_info, reserved);

      const struct v4l2_av1_quantization* quantization =
          static_cast<const struct v4l2_av1_quantization*>(
              &frame->quantization);
      CONTROL(quantization, flags);
      CONTROL(quantization, base_q_idx);
      CONTROL(quantization, delta_q_y_dc);
      CONTROL(quantization, delta_q_u_dc);
      CONTROL(quantization, delta_q_u_ac);
      CONTROL(quantization, delta_q_v_dc);
      CONTROL(quantization, delta_q_v_ac);
      CONTROL(quantization, qm_y);
      CONTROL(quantization, qm_u);
      CONTROL(quantization, qm_v);
      CONTROL(quantization, delta_q_res);

      CONTROL(frame, superres_denom);

      const struct v4l2_av1_segmentation* segmentation =
          static_cast<const struct v4l2_av1_segmentation*>(
              &frame->segmentation);
      CONTROL(segmentation, flags);
      CONTROL(segmentation, last_active_seg_id);
      CONTROL(segmentation, feature_enabled);
      CONTROL(segmentation, feature_data);

      const struct v4l2_av1_loop_filter* loop_filter =
          static_cast<const struct v4l2_av1_loop_filter*>(&frame->loop_filter);
      CONTROL(loop_filter, flags);
      CONTROL(loop_filter, level);
      CONTROL(loop_filter, sharpness);
      CONTROL(loop_filter, ref_deltas);
      CONTROL(loop_filter, mode_deltas);
      CONTROL(loop_filter, delta_lf_res);

      const struct v4l2_av1_cdef* cdef =
          static_cast<const struct v4l2_av1_cdef*>(&frame->cdef);
      CONTROL(cdef, damping_minus_3);
      CONTROL(cdef, bits);
      CONTROL(cdef, y_pri_strength);
      CONTROL(cdef, y_sec_strength);
      CONTROL(cdef, uv_pri_strength);
      CONTROL(cdef, uv_sec_strength);

      CONTROL(frame, skip_mode_frame);
      CONTROL(frame, primary_ref_frame);

      const struct v4l2_av1_loop_restoration* loop_restoration =
          static_cast<const struct v4l2_av1_loop_restoration*>(
              &frame->loop_restoration);
      CONTROL(loop_restoration, flags);
      CONTROL(loop_restoration, lr_unit_shift);
      CONTROL(loop_restoration, lr_uv_shift);
      CONTROL(loop_restoration, reserved);
      CONTROL(loop_restoration, frame_restoration_type);
      CONTROL(loop_restoration, loop_restoration_size);

      const struct v4l2_av1_global_motion* global_motion =
          static_cast<const struct v4l2_av1_global_motion*>(
              &frame->global_motion);
      CONTROL(global_motion, flags);
      CONTROL(global_motion, type);
      CONTROL(global_motion, params);
      CONTROL(global_motion, invalid);
      CONTROL(global_motion, reserved);

      CONTROL(frame, flags);
      CONTROL(frame, frame_type);
      CONTROL(frame, order_hint);
      CONTROL(frame, upscaled_width);
      CONTROL(frame, interpolation_filter);
      CONTROL(frame, tx_mode);
      CONTROL(frame, frame_width_minus_1);
      CONTROL(frame, frame_height_minus_1);
      CONTROL(frame, render_width_minus_1);
      CONTROL(frame, render_height_minus_1);
      CONTROL(frame, current_frame_id);
      CONTROL(frame, buffer_removal_time);
      CONTROL(frame, reserved);
      CONTROL(frame, order_hints);
      CONTROL(frame, reference_frame_ts);
      CONTROL(frame, ref_frame_idx);
      CONTROL(frame, refresh_frame_flags);
    } break;
  }
  return s.str();
}

static std::string PrintStatelessVP8Control(
    const struct v4l2_ext_control* ext_ctrls) {
  DCHECK_EQ(ext_ctrls->id, static_cast<uint32_t>(V4L2_CID_STATELESS_VP8_FRAME));

  std::ostringstream s;
  s << "vp8" << std::endl;
  s << "V4L2_CID_STATELESS_VP8_FRAME" << std::endl;
  const struct v4l2_ctrl_vp8_frame* ctrl =
      static_cast<const struct v4l2_ctrl_vp8_frame*>(ext_ctrls->ptr);

  const struct v4l2_vp8_segment* segment =
      static_cast<const struct v4l2_vp8_segment*>(&ctrl->segment);
  CONTROL(segment, quant_update);
  CONTROL(segment, lf_update);
  CONTROL(segment, segment_probs);
  CONTROL(segment, padding);
  CONTROL(segment, flags);

  const struct v4l2_vp8_loop_filter* lf =
      static_cast<const struct v4l2_vp8_loop_filter*>(&ctrl->lf);
  CONTROL(lf, ref_frm_delta);
  CONTROL(lf, mb_mode_delta);
  CONTROL(lf, sharpness_level);
  CONTROL(lf, level);
  CONTROL(lf, padding);
  CONTROL(lf, flags);

  const struct v4l2_vp8_quantization* quant =
      static_cast<const struct v4l2_vp8_quantization*>(&ctrl->quant);
  CONTROL(quant, y_ac_qi);
  CONTROL(quant, y_dc_delta);
  CONTROL(quant, y2_dc_delta);
  CONTROL(quant, y2_ac_delta);
  CONTROL(quant, uv_dc_delta);
  CONTROL(quant, uv_ac_delta);
  CONTROL(quant, padding);

  const struct v4l2_vp8_entropy* entropy =
      static_cast<const struct v4l2_vp8_entropy*>(&ctrl->entropy);
  // TODO(frkoenig): how should this be displayed?
  // coeff_probs[4][8][3][11]
  CONTROL(entropy, y_mode_probs);
  CONTROL(entropy, uv_mode_probs);
  CONTROL(entropy, mv_probs);
  CONTROL(entropy, padding);

  const struct v4l2_vp8_entropy_coder_state* coder_state =
      static_cast<const struct v4l2_vp8_entropy_coder_state*>(
          &ctrl->coder_state);
  CONTROL(coder_state, range);
  CONTROL(coder_state, value);
  CONTROL(coder_state, bit_count);
  CONTROL(coder_state, padding);

  CONTROL(ctrl, width);
  CONTROL(ctrl, height);
  CONTROL(ctrl, horizontal_scale);
  CONTROL(ctrl, vertical_scale);
  CONTROL(ctrl, version);
  CONTROL(ctrl, prob_skip_false);
  CONTROL(ctrl, prob_intra);
  CONTROL(ctrl, prob_last);
  CONTROL(ctrl, prob_gf);
  CONTROL(ctrl, num_dct_parts);
  CONTROL(ctrl, first_part_size);
  CONTROL(ctrl, first_part_header_bits);
  CONTROL(ctrl, dct_part_sizes);
  CONTROL(ctrl, last_frame_ts);
  CONTROL(ctrl, golden_frame_ts);
  CONTROL(ctrl, alt_frame_ts);
  CONTROL(ctrl, flags);

  return s.str();
}

static std::string PrintStatelessVP9Control(
    const struct v4l2_ext_control* ext_ctrls) {
  DCHECK_EQ(ext_ctrls->id, static_cast<uint32_t>(V4L2_CID_STATELESS_VP9_FRAME));

  std::ostringstream s;
  s << "vp9" << std::endl;
  s << "V4L2_CID_STATELESS_VP9_FRAME" << std::endl;
  const struct v4l2_ctrl_vp9_frame* ctrl =
      static_cast<const struct v4l2_ctrl_vp9_frame*>(ext_ctrls->ptr);

  const struct v4l2_vp9_loop_filter* lf =
      static_cast<const struct v4l2_vp9_loop_filter*>(&ctrl->lf);
  CONTROL(lf, ref_deltas);
  CONTROL(lf, mode_deltas);
  CONTROL(lf, level);
  CONTROL(lf, sharpness);
  CONTROL(lf, flags);
  CONTROL(lf, reserved);

  const struct v4l2_vp9_quantization* quant =
      static_cast<const struct v4l2_vp9_quantization*>(&ctrl->quant);
  CONTROL(quant, base_q_idx);
  CONTROL(quant, delta_q_y_dc);
  CONTROL(quant, delta_q_uv_dc);
  CONTROL(quant, delta_q_uv_ac);
  CONTROL(quant, reserved);

  const struct v4l2_vp9_segmentation* seg =
      static_cast<const struct v4l2_vp9_segmentation*>(&ctrl->seg);
  CONTROL(seg, feature_data);
  CONTROL(seg, feature_enabled);
  CONTROL(seg, tree_probs);
  CONTROL(seg, pred_probs);
  CONTROL(seg, flags);
  CONTROL(seg, reserved);

  CONTROL(ctrl, flags);
  CONTROL(ctrl, compressed_header_size);
  CONTROL(ctrl, uncompressed_header_size);
  CONTROL(ctrl, frame_width_minus_1);
  CONTROL(ctrl, frame_height_minus_1);
  CONTROL(ctrl, render_width_minus_1);
  CONTROL(ctrl, render_height_minus_1);
  CONTROL(ctrl, last_frame_ts);
  CONTROL(ctrl, golden_frame_ts);
  CONTROL(ctrl, alt_frame_ts);
  CONTROL(ctrl, ref_frame_sign_bias);
  CONTROL(ctrl, reset_frame_context);
  CONTROL(ctrl, frame_context_idx);
  CONTROL(ctrl, profile);
  CONTROL(ctrl, bit_depth);
  CONTROL(ctrl, interpolation_filter);
  CONTROL(ctrl, tile_cols_log2);
  CONTROL(ctrl, tile_rows_log2);
  CONTROL(ctrl, reference_mode);
  CONTROL(ctrl, reserved);

  return s.str();
}

static std::string PrintStatelessH264Control(
    const struct v4l2_ext_control* ext_ctrls) {
  std::ostringstream s;
  s << "h.264" << std::endl;
  switch (ext_ctrls->id) {
    case V4L2_CID_STATELESS_H264_SPS: {
      s << "V4L2_CID_STATELESS_H264_SPS" << std::endl;
      const struct v4l2_ctrl_h264_sps* sps =
          static_cast<const struct v4l2_ctrl_h264_sps*>(ext_ctrls->ptr);
      CONTROL(sps, profile_idc);
      CONTROL(sps, constraint_set_flags);
      CONTROL(sps, level_idc);
      CONTROL(sps, seq_parameter_set_id);
      CONTROL(sps, chroma_format_idc);
      CONTROL(sps, bit_depth_luma_minus8);
      CONTROL(sps, bit_depth_chroma_minus8);
      CONTROL(sps, log2_max_frame_num_minus4);
      CONTROL(sps, pic_order_cnt_type);
      CONTROL(sps, log2_max_pic_order_cnt_lsb_minus4);
      CONTROL(sps, max_num_ref_frames);
      CONTROL(sps, num_ref_frames_in_pic_order_cnt_cycle);
      CONTROL(sps, offset_for_ref_frame);
      CONTROL(sps, offset_for_non_ref_pic);
      CONTROL(sps, offset_for_top_to_bottom_field);
      CONTROL(sps, pic_width_in_mbs_minus1);
      CONTROL(sps, pic_height_in_map_units_minus1);
      CONTROL(sps, flags);
    } break;
    case V4L2_CID_STATELESS_H264_PPS: {
      s << "V4L2_CID_STATELESS_H264_PPS" << std::endl;
      const struct v4l2_ctrl_h264_pps* pps =
          static_cast<const struct v4l2_ctrl_h264_pps*>(ext_ctrls->ptr);
      CONTROL(pps, pic_parameter_set_id);
      CONTROL(pps, seq_parameter_set_id);
      CONTROL(pps, num_slice_groups_minus1);
      CONTROL(pps, num_ref_idx_l0_default_active_minus1);
      CONTROL(pps, num_ref_idx_l1_default_active_minus1);
      CONTROL(pps, weighted_bipred_idc);
      CONTROL(pps, pic_init_qp_minus26);
      CONTROL(pps, pic_init_qs_minus26);
      CONTROL(pps, chroma_qp_index_offset);
      CONTROL(pps, second_chroma_qp_index_offset);
      CONTROL(pps, flags);
    } break;
    case V4L2_CID_STATELESS_H264_SCALING_MATRIX: {
      s << "V4L2_CID_STATELESS_H264_SCALING_MATRIX" << std::endl;
      const struct v4l2_ctrl_h264_scaling_matrix* sm =
          static_cast<const struct v4l2_ctrl_h264_scaling_matrix*>(
              ext_ctrls->ptr);
      CONTROL(sm, scaling_list_4x4);
      CONTROL(sm, scaling_list_8x8);
    } break;
    case V4L2_CID_STATELESS_H264_DECODE_PARAMS: {
      s << "V4L2_CID_STATELESS_H264_DECODE_PARAMS" << std::endl;
      const struct v4l2_ctrl_h264_decode_params* dp =
          static_cast<const struct v4l2_ctrl_h264_decode_params*>(
              ext_ctrls->ptr);
      for (uint32_t i = 0; i < 16; ++i) {
        s << "dbp entry " << +i << std::endl;
        const struct v4l2_h264_dpb_entry* dpb =
            static_cast<const struct v4l2_h264_dpb_entry*>(&dp->dpb[i]);
        CONTROL(dpb, reference_ts);
        CONTROL(dpb, pic_num);
        CONTROL(dpb, frame_num);
        CONTROL(dpb, fields);
        CONTROL(dpb, top_field_order_cnt);
        CONTROL(dpb, bottom_field_order_cnt);
        CONTROL(dpb, flags);
      }

      CONTROL(dp, nal_ref_idc);
      CONTROL(dp, frame_num);
      CONTROL(dp, top_field_order_cnt);
      CONTROL(dp, bottom_field_order_cnt);
      CONTROL(dp, idr_pic_id);
      CONTROL(dp, pic_order_cnt_lsb);
      CONTROL(dp, delta_pic_order_cnt_bottom);
      CONTROL(dp, delta_pic_order_cnt0);
      CONTROL(dp, delta_pic_order_cnt1);
      CONTROL(dp, dec_ref_pic_marking_bit_size);
      CONTROL(dp, pic_order_cnt_bit_size);
      CONTROL(dp, slice_group_change_cycle);
      CONTROL(dp, flags);
    } break;
    case V4L2_CID_STATELESS_H264_DECODE_MODE: {
      const enum v4l2_stateless_h264_decode_mode dm =
          static_cast<const enum v4l2_stateless_h264_decode_mode>(
              ext_ctrls->value);
      s << "V4L2_CID_STATELESS_H264_DECODE_MODE: "
        << (dm == V4L2_STATELESS_H264_DECODE_MODE_SLICE_BASED
                ? "V4L2_STATELESS_H264_DECODE_MODE_SLICE_BASED"
                : "V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED")
        << std::endl;
    } break;
  }

  return s.str();
}

static std::string PrintStatelessHEVCControl(
    const struct v4l2_ext_control* ext_ctrls) {
  std::ostringstream s;
  s << "hevc" << std::endl;
  switch (ext_ctrls->id) {
    case V4L2_CID_STATELESS_HEVC_SPS: {
      const struct v4l2_ctrl_hevc_sps* sps =
          static_cast<const struct v4l2_ctrl_hevc_sps*>(ext_ctrls->ptr);
      CONTROL(sps, video_parameter_set_id);
      CONTROL(sps, seq_parameter_set_id);
      CONTROL(sps, pic_width_in_luma_samples);
      CONTROL(sps, pic_height_in_luma_samples);
      CONTROL(sps, bit_depth_luma_minus8);
      CONTROL(sps, bit_depth_chroma_minus8);
      CONTROL(sps, log2_max_pic_order_cnt_lsb_minus4);
      CONTROL(sps, sps_max_dec_pic_buffering_minus1);
      CONTROL(sps, sps_max_num_reorder_pics);
      CONTROL(sps, sps_max_latency_increase_plus1);
      CONTROL(sps, log2_min_luma_coding_block_size_minus3);
      CONTROL(sps, log2_diff_max_min_luma_coding_block_size);
      CONTROL(sps, log2_min_luma_transform_block_size_minus2);
      CONTROL(sps, log2_diff_max_min_luma_transform_block_size);
      CONTROL(sps, max_transform_hierarchy_depth_inter);
      CONTROL(sps, max_transform_hierarchy_depth_intra);
      CONTROL(sps, pcm_sample_bit_depth_luma_minus1);
      CONTROL(sps, pcm_sample_bit_depth_chroma_minus1);
      CONTROL(sps, log2_min_pcm_luma_coding_block_size_minus3);
      CONTROL(sps, log2_diff_max_min_pcm_luma_coding_block_size);
      CONTROL(sps, num_short_term_ref_pic_sets);
      CONTROL(sps, num_long_term_ref_pics_sps);
      CONTROL(sps, chroma_format_idc);
      CONTROL(sps, sps_max_sub_layers_minus1);
      CONTROL(sps, flags);
    } break;
    case V4L2_CID_STATELESS_HEVC_PPS: {
      const struct v4l2_ctrl_hevc_pps* pps =
          static_cast<const struct v4l2_ctrl_hevc_pps*>(ext_ctrls->ptr);
      CONTROL(pps, pic_parameter_set_id);
      CONTROL(pps, num_extra_slice_header_bits);
      CONTROL(pps, num_ref_idx_l0_default_active_minus1);
      CONTROL(pps, num_ref_idx_l1_default_active_minus1);
      CONTROL(pps, init_qp_minus26);
      CONTROL(pps, diff_cu_qp_delta_depth);
      CONTROL(pps, pps_cb_qp_offset);
      CONTROL(pps, pps_cr_qp_offset);
      CONTROL(pps, num_tile_columns_minus1);
      CONTROL(pps, num_tile_rows_minus1);
      CONTROL(pps, column_width_minus1);
      CONTROL(pps, row_height_minus1);
      CONTROL(pps, pps_beta_offset_div2);
      CONTROL(pps, pps_tc_offset_div2);
      CONTROL(pps, log2_parallel_merge_level_minus2);
      CONTROL(pps, reserved);
      CONTROL(pps, flags);
    } break;
    case V4L2_CID_STATELESS_HEVC_SCALING_MATRIX: {
      const struct v4l2_ctrl_hevc_scaling_matrix* sm =
          static_cast<const struct v4l2_ctrl_hevc_scaling_matrix*>(
              ext_ctrls->ptr);
      CONTROL(sm, scaling_list_4x4);
      CONTROL(sm, scaling_list_8x8);
      CONTROL(sm, scaling_list_16x16);
      CONTROL(sm, scaling_list_32x32);
      CONTROL(sm, scaling_list_dc_coef_16x16);
      CONTROL(sm, scaling_list_dc_coef_32x32);
    } break;
    case V4L2_CID_STATELESS_HEVC_DECODE_PARAMS: {
      const struct v4l2_ctrl_hevc_decode_params* dp =
          static_cast<const struct v4l2_ctrl_hevc_decode_params*>(
              ext_ctrls->ptr);
      CONTROL(dp, pic_order_cnt_val);
      CONTROL(dp, short_term_ref_pic_set_size);
      CONTROL(dp, long_term_ref_pic_set_size);
      CONTROL(dp, num_active_dpb_entries);
      CONTROL(dp, num_poc_st_curr_before);
      CONTROL(dp, num_poc_st_curr_after);
      CONTROL(dp, num_poc_lt_curr);
      CONTROL(dp, poc_st_curr_before);
      CONTROL(dp, poc_st_curr_after);
      CONTROL(dp, poc_lt_curr);
      CONTROL(dp, num_delta_pocs_of_ref_rps_idx);
      for (uint32_t i = 0; i < V4L2_HEVC_DPB_ENTRIES_NUM_MAX; ++i) {
        s << "dbp entry " << +i << std::endl;
        const struct v4l2_hevc_dpb_entry* dpb =
            static_cast<const struct v4l2_hevc_dpb_entry*>(&dp->dpb[i]);
        CONTROL(dpb, timestamp);
        CONTROL(dpb, flags);
        CONTROL(dpb, field_pic);
        CONTROL(dpb, reserved);
        CONTROL(dpb, pic_order_cnt_val);
      }
      CONTROL(dp, flags);
    } break;
  }

  return s.str();
}

std::string V4L2ControlsToString(const struct v4l2_ext_controls* ctrls) {
  const struct v4l2_ext_control* ext_ctrls = ctrls->controls;
  std::ostringstream s;

  CONTROL(ctrls, which);
  CONTROL(ctrls, count);
  CONTROL(ctrls, error_idx);
  CONTROL(ctrls, request_fd);
  CONTROL(ctrls, reserved);

  for (uint32_t i = 0; i < ctrls->count; ++i) {
    switch (ext_ctrls->id) {
      case V4L2_CID_STATELESS_AV1_SEQUENCE:
      case V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY:
      case V4L2_CID_STATELESS_AV1_FRAME:
        s << PrintStatelessAV1Control(ext_ctrls);
        break;
      case V4L2_CID_STATELESS_VP8_FRAME:
        s << PrintStatelessVP8Control(ext_ctrls);
        break;
      case V4L2_CID_STATELESS_VP9_FRAME:
        s << PrintStatelessVP9Control(ext_ctrls);
        break;
      case V4L2_CID_STATELESS_H264_SPS:
      case V4L2_CID_STATELESS_H264_PPS:
      case V4L2_CID_STATELESS_H264_SCALING_MATRIX:
      case V4L2_CID_STATELESS_H264_DECODE_PARAMS:
      case V4L2_CID_STATELESS_H264_DECODE_MODE:
        s << PrintStatelessH264Control(ext_ctrls);
        break;
      case V4L2_CID_STATELESS_HEVC_SPS:
      case V4L2_CID_STATELESS_HEVC_PPS:
      case V4L2_CID_STATELESS_HEVC_SCALING_MATRIX:
      case V4L2_CID_STATELESS_HEVC_DECODE_PARAMS:
        s << PrintStatelessHEVCControl(ext_ctrls);
        break;
      default:
        s << "Unknown Control: 0x" << std::hex << ext_ctrls->id << std::endl;
    }

    ext_ctrls++;
  }

  return s.str();
}
#else
std::string V4L2ControlsToString(const struct v4l2_ext_controls* ctrls) {
  return "Use a debug build to see controls.";
}
#endif
}  // namespace media
