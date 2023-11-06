// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/device.h"

#include <map>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#endif

#include <fcntl.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/utils.h"
#include "media/gpu/v4l2/v4l2_utils.h"

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME v4l2_fourcc('A', 'V', '1', 'F')
#endif

namespace media {

namespace {

// Helper functions for translating between V4L2 structs that should not
// be included in the header and external structs that are shared.
enum v4l2_buf_type BufferTypeToV4L2(BufferType type) {
  if (type == BufferType::kCompressedData) {
    return V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  } else if (type == BufferType::kRawFrames) {
    return V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  }

  NOTREACHED();
  return V4L2_BUF_TYPE_PRIVATE;
}

enum v4l2_memory MemoryTypeToV4L2(MemoryType memory) {
  switch (memory) {
    case MemoryType::kMemoryMapped:
      return V4L2_MEMORY_MMAP;
    case MemoryType::kDmaBuf:
      return V4L2_MEMORY_DMABUF;
    case MemoryType::kInvalid:
      NOTREACHED();
      // V4L2_MEMORY_USERPTR is not used in our code.
      return V4L2_MEMORY_USERPTR;
  }
}

BufferType V4L2ToBufferType(unsigned int type) {
  if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
    return BufferType::kCompressedData;
  } else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
    return BufferType::kRawFrames;
  }

  NOTREACHED();
  return BufferType::kInvalid;
}

MemoryType V4L2ToMemoryType(unsigned int memory) {
  switch (memory) {
    case V4L2_MEMORY_MMAP:
      return MemoryType::kMemoryMapped;
    case V4L2_MEMORY_DMABUF:
      return MemoryType::kDmaBuf;
  }

  NOTREACHED();
  return MemoryType::kInvalid;
}

Buffer V4L2BufferToBuffer(const struct v4l2_buffer& v4l2_buffer) {
  const BufferType buffer_type = V4L2ToBufferType(v4l2_buffer.type);
  const MemoryType memory_type = V4L2ToMemoryType(v4l2_buffer.memory);
  Buffer buffer(buffer_type, memory_type, v4l2_buffer.index,
                v4l2_buffer.length);
  for (uint32_t plane = 0; plane < buffer.PlaneCount(); ++plane) {
    buffer.SetupPlane(plane, v4l2_buffer.m.planes[plane].m.mem_offset,
                      v4l2_buffer.m.planes[plane].length);
  }

  return buffer;
}

using v4l2_enum_type = decltype(V4L2_PIX_FMT_H264);
// Correspondence from V4L2 codec described as a pixel format to a Control ID.
static const std::map<v4l2_enum_type, v4l2_enum_type>
    kV4L2CodecPixFmtToProfileCID = {
        {V4L2_PIX_FMT_H264_SLICE, V4L2_CID_MPEG_VIDEO_H264_PROFILE},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_HEVC_SLICE, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_PIX_FMT_VP8_FRAME, V4L2_CID_MPEG_VIDEO_VP8_PROFILE},
        {V4L2_PIX_FMT_VP9_FRAME, V4L2_CID_MPEG_VIDEO_VP9_PROFILE},
#if BUILDFLAG(IS_CHROMEOS)
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
        {V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, {HEVCPROFILE_MAIN}},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        {V4L2_CID_MPEG_VIDEO_VP8_PROFILE, {VP8PROFILE_ANY}},
        {V4L2_CID_MPEG_VIDEO_VP9_PROFILE, {VP9PROFILE_PROFILE0}},
#if BUILDFLAG(IS_CHROMEOS)
        {V4L2_CID_MPEG_VIDEO_AV1_PROFILE, {AV1PROFILE_PROFILE_MAIN}},
#endif
};

VideoCodec V4L2PixFmtToVideoCodec(uint32_t pix_fmt) {
  switch (pix_fmt) {
    case V4L2_PIX_FMT_H264_SLICE:
      return VideoCodec::kH264;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_PIX_FMT_HEVC_SLICE:
      return VideoCodec::kHEVC;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case V4L2_PIX_FMT_VP8_FRAME:
      return VideoCodec::kVP8;
    case V4L2_PIX_FMT_VP9_FRAME:
      return VideoCodec::kVP9;
    case V4L2_PIX_FMT_AV1_FRAME:
      return VideoCodec::kAV1;
  }
  return VideoCodec::kUnknown;
}

}  // namespace

Device::Device() {}

Buffer::Buffer(BufferType buffer_type,
               MemoryType memory_type,
               uint32_t index,
               uint32_t plane_count)
    : buffer_type_(buffer_type), memory_type_(memory_type), index_(index) {
  planes_.resize(plane_count);
}

Buffer::Buffer(const Buffer&) = default;

void* Buffer::MappedAddress(uint32_t plane) const {
  DCHECK(memory_type_ == MemoryType::kMemoryMapped);
  return planes_[plane].mapped_address;
}

void Buffer::SetMappedAddress(uint32_t plane, void* address) {
  DVLOGF(4) << plane << " : " << address;
  DCHECK(memory_type_ == MemoryType::kMemoryMapped);
  planes_[plane].mapped_address = address;
}

void Buffer::SetupPlane(uint32_t plane, size_t offset, size_t size) {
  planes_[plane].mem_offset = offset;
  planes_[plane].length = size;
}

bool Buffer::CopyDataIn(const void* data, size_t length) {
  DVLOGF(4) << MappedAddress(0) << " : " << data << " : " << length;

  void* destination_addr = MappedAddress(0);
  if (!destination_addr || !data || (planes_.size() != 1) ||
      (length > planes_[0].length)) {
    DVLOGF(1) << "Requirements to copy buffer failed.";
    return false;
  }

  memcpy(destination_addr, data, length);
  planes_[0].bytes_used = length;

  return true;
}

Buffer::~Buffer() {}

void Device::Close() {
  device_fd_.reset();
}

// VIDIOC_ENUM_FMT
std::set<VideoCodec> Device::EnumerateInputFormats() {
  std::set<VideoCodec> pix_fmts;
  v4l2_fmtdesc fmtdesc = {.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE};
  for (; IoctlDevice(VIDIOC_ENUM_FMT, &fmtdesc) == kIoctlOk; ++fmtdesc.index) {
    DVLOGF(4) << "Enumerated codec: "
              << media::FourccToString(fmtdesc.pixelformat) << " ("
              << fmtdesc.description << ")";
    VideoCodec enumerated_codec = V4L2PixFmtToVideoCodec(fmtdesc.pixelformat);

    // Not all codecs returned from the device are supported by ChromeOS
    if (VideoCodec::kUnknown != enumerated_codec) {
      pix_fmts.insert(enumerated_codec);
    }
  }

  return pix_fmts;
}

// VIDIOC_S_FMT
bool Device::SetInputFormat(VideoCodec codec,
                            gfx::Size resolution,
                            size_t encoded_buffer_size) {
  DVLOGF(4);
  const uint32_t pix_fmt = VideoCodecToV4L2PixFmt(codec);
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat = pix_fmt;
  format.fmt.pix_mp.width = resolution.width();
  format.fmt.pix_mp.height = resolution.height();
  format.fmt.pix_mp.num_planes = 1;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = encoded_buffer_size;

  if (IoctlDevice(VIDIOC_S_FMT, &format) != kIoctlOk ||
      format.fmt.pix_mp.pixelformat != pix_fmt) {
    DVLOGF(1) << "Failed to set format fourcc: " << FourccToString(pix_fmt);

    return false;
  }

  return true;
}

// VIDIOC_STREAMON
bool Device::StreamOn(BufferType type) {
  enum v4l2_buf_type buf_type = BufferTypeToV4L2(type);

  const int ret = IoctlDevice(VIDIOC_STREAMON, &buf_type);
  if (ret) {
    DVLOGF(1) << "VIDIOC_STREAMON failed: " << ret;
    return false;
  }
  return true;
}

// VIDIOC_STREAMOFF
bool Device::StreamOff(BufferType type) {
  enum v4l2_buf_type buf_type = BufferTypeToV4L2(type);

  const int ret = IoctlDevice(VIDIOC_STREAMOFF, &buf_type);
  if (ret) {
    DVLOGF(1) << "VIDIOC_STREAMOFF failed: " << ret;
    return false;
  }
  return true;
}

// VIDIOC_REQBUFS
absl::optional<uint32_t> Device::RequestBuffers(BufferType type,
                                                MemoryType memory,
                                                size_t count) {
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));

  reqbufs.count = count;
  reqbufs.type = BufferTypeToV4L2(type);
  reqbufs.memory = MemoryTypeToV4L2(memory);

  const int ret = IoctlDevice(VIDIOC_REQBUFS, &reqbufs);
  if (ret) {
    DVLOGF(1) << "Failed to allocate " << count
              << " input buffers with VIDIOC_REQBUFS: " << ret;
    return absl::nullopt;
  }

  return reqbufs.count;
}

// VIDIOC_QUERYBUF
absl::optional<Buffer> Device::QueryBuffer(BufferType buffer_type,
                                           MemoryType memory_type,
                                           uint32_t index,
                                           uint32_t num_planes) {
  struct v4l2_buffer v4l2_buffer;
  struct v4l2_plane v4l2_planes[VIDEO_MAX_PLANES];
  memset(&v4l2_buffer, 0, sizeof(v4l2_buffer));
  memset(v4l2_planes, 0, sizeof(v4l2_planes));
  v4l2_buffer.m.planes = v4l2_planes;
  v4l2_buffer.length = num_planes;

  v4l2_buffer.type = BufferTypeToV4L2(buffer_type);
  v4l2_buffer.memory = MemoryTypeToV4L2(memory_type);
  v4l2_buffer.index = index;

  const int ret = IoctlDevice(VIDIOC_QUERYBUF, &v4l2_buffer);
  if (ret) {
    DVLOGF(1) << "VIDIOC_QUERYBUF failed: ";
    return absl::nullopt;
  }

  return V4L2BufferToBuffer(v4l2_buffer);
}

// VIDIOC_ENUM_FRAMESIZES
std::pair<gfx::Size, gfx::Size> Device::GetFrameResolutionRange(
    VideoCodec codec) {
  constexpr gfx::Size kDefaultMaxCodedSize(1920, 1088);
  constexpr gfx::Size kDefaultMinCodedSize(16, 16);

  v4l2_frmsizeenum frame_size;
  memset(&frame_size, 0, sizeof(frame_size));
  frame_size.pixel_format = VideoCodecToV4L2PixFmt(codec);
  if (IoctlDevice(VIDIOC_ENUM_FRAMESIZES, &frame_size) == kIoctlOk) {
#if BUILDFLAG(IS_CHROMEOS)
    // All of Chrome-supported implementations support STEPWISE only.
    CHECK_EQ(frame_size.type, V4L2_FRMSIZE_TYPE_STEPWISE);
#endif
    if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
      return std::make_pair(gfx::Size(frame_size.stepwise.min_width,
                                      frame_size.stepwise.min_height),
                            gfx::Size(frame_size.stepwise.max_width,
                                      frame_size.stepwise.max_height));
    }
  }

  VPLOGF(1) << "VIDIOC_ENUM_FRAMESIZES failed for "
            << media::FourccToString(frame_size.pixel_format)
            << ", using default values";
  return std::make_pair(kDefaultMinCodedSize, kDefaultMaxCodedSize);
}

// VIDIOC_QUERYCTRL, VIDIOC_QUERYMENU
std::vector<VideoCodecProfile> Device::ProfilesForVideoCodec(VideoCodec codec) {
  const uint32_t pix_fmt = VideoCodecToV4L2PixFmt(codec);

  if (!base::Contains(kV4L2CodecPixFmtToProfileCID, pix_fmt)) {
    // This is OK: there are many codecs that are not supported by Chrome.
    DVLOGF(4) << "Unsupported codec: " << FourccToString(pix_fmt);
    return {};
  }

  const auto profile_cid = kV4L2CodecPixFmtToProfileCID.at(pix_fmt);
  v4l2_queryctrl query_ctrl = {.id = base::strict_cast<__u32>(profile_cid)};
  if (IoctlDevice(VIDIOC_QUERYCTRL, &query_ctrl) != kIoctlOk) {
    return {};
  }

  std::vector<VideoCodecProfile> profiles;

  v4l2_querymenu query_menu = {
      .id = query_ctrl.id,
      .index = base::checked_cast<__u32>(query_ctrl.minimum)};
  for (; query_menu.index <= base::checked_cast<__u32>(query_ctrl.maximum);
       query_menu.index++) {
    if (IoctlDevice(VIDIOC_QUERYMENU, &query_menu) != kIoctlOk) {
      continue;
    }
    const VideoCodecProfile profile =
        V4L2ProfileToVideoCodecProfile(profile_cid, query_menu.index);
    DVLOGF_IF(4, profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        << "Profile: " << query_menu.name << " for " << FourccToString(pix_fmt)
        << " not supported by Chrome, skipping.";

    if (profile != VIDEO_CODEC_PROFILE_UNKNOWN) {
      profiles.push_back(profile);
      DVLOGF(4) << FourccToString(pix_fmt) << " profile " << query_menu.name
                << " supported.";
    }
  }

  // Erase duplicated profiles. This is needed because H264PROFILE_BASELINE maps
  // to both V4L2_MPEG_VIDEO_H264_PROFILE__BASELINE/CONSTRAINED_BASELINE
  base::ranges::sort(profiles);
  profiles.erase(base::ranges::unique(profiles), profiles.end());
  return profiles;
}

bool Device::OpenDevice() {
  DVLOGF(3);
  static const std::string kDecoderDevicePrefix = "/dev/video-dec";

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  for (int i = 0; i < 10; ++i) {
    const auto path = kDecoderDevicePrefix + base::NumberToString(i);
    device_fd_.reset(
        HANDLE_EINTR(open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC, 0)));
    if (!device_fd_.is_valid()) {
      VPLOGF(2) << "Failed to open media device: " << path;
      continue;
    }

    break;
  }

  if (!device_fd_.is_valid()) {
    DVLOGF(1) << "Failed to open device fd.";
    return false;
  }

  return true;
}

bool Device::MmapBuffer(Buffer& buffer) {
  for (uint32_t plane = 0; plane < buffer.PlaneCount(); ++plane) {
    void* p = mmap(nullptr, buffer.PlaneLength(plane), PROT_READ | PROT_WRITE,
                   MAP_SHARED, device_fd_.get(), buffer.PlaneMemOffset(plane));
    // dealloc rest if failed
    if (p == MAP_FAILED) {
      VPLOGF(1) << "mmap() failed: ";
      return false;
    }
    buffer.SetMappedAddress(plane, p);
  }

  return true;
}

void Device::MunmapBuffer(Buffer& buffer) {
  for (uint32_t plane = 0; plane < buffer.PlaneCount(); plane++) {
    if (buffer.MappedAddress(plane) != nullptr) {
      munmap(buffer.MappedAddress(plane), buffer.PlaneLength(plane));
      buffer.SetMappedAddress(plane, nullptr);
    }
  }
}

Device::~Device() {}

int Device::Ioctl(const base::ScopedFD& fd, uint64_t request, void* arg) {
  DCHECK(fd.is_valid());
  const int ret = HANDLE_EINTR(ioctl(fd.get(), request, arg));
  if (ret != kIoctlOk) {
    const logging::SystemErrorCode err = logging::GetLastSystemErrorCode();
    if (err == EAGAIN && request == VIDIOC_DQBUF) {
      DVLOGF(4) << IoctlToString(request)
                << " failed: " << logging::SystemErrorCodeToString(err)
                << ": This is _usually_ an expected failure from trying to "
                   "VIDIOC_DQBUF a buffer that is not done being processed.";
    } else {
      DVLOGF(1) << IoctlToString(request)
                << " failed: " << logging::SystemErrorCodeToString(err);
    }
  }
  return ret;
}

int Device::IoctlDevice(uint64_t request, void* arg) {
  return Ioctl(device_fd_, request, arg);
}

}  //  namespace media
