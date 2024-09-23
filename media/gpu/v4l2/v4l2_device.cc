// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_device.h"

#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/base/color_plane_layout.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_queue.h"
#include "media/gpu/v4l2/v4l2_utils.h"

namespace media {

namespace {

uint32_t V4L2PixFmtToDrmFormat(uint32_t format) {
  switch (format) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return DRM_FORMAT_NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return DRM_FORMAT_YUV420;

    case V4L2_PIX_FMT_YVU420:
      return DRM_FORMAT_YVU420;

    case V4L2_PIX_FMT_RGB32:
      return DRM_FORMAT_ARGB8888;

    default:
      DVLOGF(1) << "Unrecognized format " << FourccToString(format);
      return 0;
  }
}

}  // namespace

// This class is used to expose V4L2Queue's constructor to this module. This is
// to ensure that nobody else can create instances of it.
class V4L2QueueFactory {
 public:
  static scoped_refptr<V4L2Queue> CreateQueue(scoped_refptr<V4L2Device> dev,
                                              enum v4l2_buf_type type,
                                              base::OnceClosure destroy_cb) {
    return new V4L2Queue(base::BindRepeating(&V4L2Device::Ioctl, dev),
                         base::BindRepeating(&V4L2Device::SchedulePoll, dev),
                         base::BindRepeating(&V4L2Device::Mmap, dev),
                         dev->get_secure_allocate_cb(), type,
                         std::move(destroy_cb));
  }
};

V4L2Device::V4L2Device() {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
}

V4L2Device::~V4L2Device() {
  CloseDevice();
}

scoped_refptr<V4L2Queue> V4L2Device::GetQueue(enum v4l2_buf_type type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  switch (type) {
    // Supported queue types.
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
      break;
    default:
      VLOGF(1) << "Unsupported V4L2 queue type: " << type;
      return nullptr;
  }

  // TODO(acourbot): we should instead query the device for available queues,
  // and allocate them accordingly. This will do for now though.
  auto it = queues_.find(type);
  if (it != queues_.end())
    return scoped_refptr<V4L2Queue>(it->second);

  scoped_refptr<V4L2Queue> queue = V4L2QueueFactory::CreateQueue(
      this, type, base::BindOnce(&V4L2Device::OnQueueDestroyed, this, type));

  queues_[type] = queue.get();
  return queue;
}

void V4L2Device::OnQueueDestroyed(v4l2_buf_type buf_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  auto it = queues_.find(buf_type);
  CHECK(it != queues_.end(), base::NotFatalUntil::M130);
  queues_.erase(it);
}

bool V4L2Device::Open(Type type, uint32_t v4l2_pixfmt) {
  DVLOGF(3);
  std::string path = GetDevicePathFor(type, v4l2_pixfmt);

  if (path.empty()) {
    VLOGF(1) << "No devices supporting " << FourccToString(v4l2_pixfmt)
             << " for type: " << static_cast<int>(type);
    return false;
  }

  if (!OpenDevicePath(path)) {
    VLOGF(1) << "Failed opening " << path;
    return false;
  }

  device_poll_interrupt_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
  if (!device_poll_interrupt_fd_.is_valid()) {
    VLOGF(1) << "Failed creating a poll interrupt fd";
    return false;
  }

  return true;
}

bool V4L2Device::IsValid() {
  return device_poll_interrupt_fd_.is_valid();
}

std::string V4L2Device::GetDriverName() {
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  if (Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP"
              << ", caps check failed: 0x" << std::hex << caps.capabilities;
    return "";
  }

  return std::string(reinterpret_cast<const char*>(caps.driver));
}

// static
int32_t V4L2Device::VideoCodecProfileToV4L2H264Profile(
    VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
    case H264PROFILE_MAIN:
      return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
    case H264PROFILE_EXTENDED:
      return V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED;
    case H264PROFILE_HIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10;
    case H264PROFILE_HIGH422PROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE;
    case H264PROFILE_SCALABLEBASELINE:
      return V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_BASELINE;
    case H264PROFILE_SCALABLEHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_SCALABLE_HIGH;
    case H264PROFILE_STEREOHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH;
    case H264PROFILE_MULTIVIEWHIGH:
      return V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH;
    default:
      DVLOGF(1) << "Add more cases as needed";
      return -1;
  }
}

// static
int32_t V4L2Device::H264LevelIdcToV4L2H264Level(uint8_t level_idc) {
  switch (level_idc) {
    case 10:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
    case 9:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1B;
    case 11:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;
    case 12:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;
    case 13:
      return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;
    case 20:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;
    case 21:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;
    case 22:
      return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;
    case 30:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;
    case 31:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;
    case 32:
      return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;
    case 40:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
    case 41:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
    case 42:
      return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;
    case 50:
      return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
    case 51:
      return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
    default:
      DVLOGF(1) << "Unrecognized level_idc: " << static_cast<int>(level_idc);
      return -1;
  }
}

// static
gfx::Size V4L2Device::AllocatedSizeFromV4L2Format(
    const struct v4l2_format& format) {
  gfx::Size coded_size;
  gfx::Size visible_size;
  VideoPixelFormat frame_format = PIXEL_FORMAT_UNKNOWN;
  size_t bytesperline = 0;
  // Total bytes in the frame.
  size_t sizeimage = 0;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    DCHECK_GT(format.fmt.pix_mp.num_planes, 0);
    bytesperline =
        base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[0].bytesperline);
    for (size_t i = 0; i < format.fmt.pix_mp.num_planes; ++i) {
      sizeimage +=
          base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[i].sizeimage);
    }
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix_mp.width),
                         base::checked_cast<int>(format.fmt.pix_mp.height));
    const uint32_t pix_fmt = format.fmt.pix_mp.pixelformat;
    const auto frame_fourcc = Fourcc::FromV4L2PixFmt(pix_fmt);
    if (!frame_fourcc) {
      VLOGF(1) << "Unsupported format " << FourccToString(pix_fmt);
      return coded_size;
    }
    frame_format = frame_fourcc->ToVideoPixelFormat();
  } else {
    bytesperline = base::checked_cast<int>(format.fmt.pix.bytesperline);
    sizeimage = base::checked_cast<int>(format.fmt.pix.sizeimage);
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix.width),
                         base::checked_cast<int>(format.fmt.pix.height));
    const uint32_t fourcc = format.fmt.pix.pixelformat;
    const auto frame_fourcc = Fourcc::FromV4L2PixFmt(fourcc);
    if (!frame_fourcc) {
      VLOGF(1) << "Unsupported format " << FourccToString(fourcc);
      return coded_size;
    }
    frame_format = frame_fourcc ? frame_fourcc->ToVideoPixelFormat()
                                : PIXEL_FORMAT_UNKNOWN;
  }

  // V4L2 does not provide per-plane bytesperline (bpl) when different
  // components are sharing one physical plane buffer. In this case, it only
  // provides bpl for the first component in the plane. So we can't depend on it
  // for calculating height, because bpl may vary within one physical plane
  // buffer. For example, YUV420 contains 3 components in one physical plane,
  // with Y at 8 bits per pixel, and Cb/Cr at 4 bits per pixel per component,
  // but we only get 8 pits per pixel from bytesperline in physical plane 0.
  // So we need to get total frame bpp from elsewhere to calculate coded height.

  // We need bits per pixel for one component only to calculate
  // coded_width from bytesperline.
  int plane_horiz_bits_per_pixel =
      VideoFrame::PlaneHorizontalBitsPerPixel(frame_format, 0);

  // Adding up bpp for each component will give us total bpp for all components.
  int total_bpp = 0;
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame_format); ++i)
    total_bpp += VideoFrame::PlaneBitsPerPixel(frame_format, i);

  if (sizeimage == 0 || bytesperline == 0 || plane_horiz_bits_per_pixel == 0 ||
      total_bpp == 0 || (bytesperline * 8) % plane_horiz_bits_per_pixel != 0) {
    VLOGF(1) << "Invalid format provided";
    return coded_size;
  }

  // Coded width can be calculated by taking the first component's bytesperline,
  // which in V4L2 always applies to the first component in physical plane
  // buffer.
  int coded_width = bytesperline * 8 / plane_horiz_bits_per_pixel;
  // Sizeimage is coded_width * coded_height * total_bpp. In the case that we
  // don't have exact alignment due to padding in the driver, round up so that
  // the buffer is large enough.
  std::div_t res = std::div(sizeimage * 8, coded_width * total_bpp);
  int coded_height = res.quot + std::min(res.rem, 1);

  coded_size.SetSize(coded_width, coded_height);
  DVLOGF(3) << "coded_size=" << coded_size.ToString();

  // Sanity checks. Calculated coded size has to contain given visible size
  // and fulfill buffer byte size requirements.
  DCHECK(gfx::Rect(coded_size).Contains(gfx::Rect(visible_size)));
  DCHECK_LE(sizeimage, VideoFrame::AllocationSize(frame_format, coded_size));

  return coded_size;
}

int V4L2Device::Ioctl(int request, void* arg) {
  DCHECK(device_fd_.is_valid());
  return HANDLE_EINTR(ioctl(device_fd_.get(), request, arg));
}

bool V4L2Device::Poll(bool poll_device, bool* event_pending) {
  struct pollfd pollfds[2];
  nfds_t nfds;
  int pollfd = -1;

  pollfds[0].fd = device_poll_interrupt_fd_.get();
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_device) {
    DVLOGF(5) << "adding device fd to poll() set";
    pollfds[nfds].fd = device_fd_.get();
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    pollfd = nfds;
    nfds++;
  }

  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    VPLOGF(1) << "poll() failed";
    return false;
  }
  *event_pending = (pollfd != -1 && pollfds[pollfd].revents & POLLPRI);
  return true;
}

void* V4L2Device::Mmap(void* addr,
                       unsigned int len,
                       int prot,
                       int flags,
                       unsigned int offset) {
  DCHECK(device_fd_.is_valid());
  return mmap(addr, len, prot, flags, device_fd_.get(), offset);
}

void V4L2Device::Munmap(void* addr, unsigned int len) {
  munmap(addr, len);
}

bool V4L2Device::SetDevicePollInterrupt() {
  DVLOGF(4);

  const uint64_t buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    VPLOGF(1) << "write() failed";
    return false;
  }
  return true;
}

bool V4L2Device::ClearDevicePollInterrupt() {
  DVLOGF(5);

  uint64_t buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_.get(), &buf, sizeof(buf))) ==
      -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      VPLOGF(1) << "read() failed";
      return false;
    }
  }
  return true;
}

bool V4L2Device::CanCreateEGLImageFrom(const Fourcc fourcc) const {
  static uint32_t kEGLImageDrmFmtsSupported[] = {
    DRM_FORMAT_ARGB8888,
#if defined(ARCH_CPU_ARM_FAMILY)
    DRM_FORMAT_NV12,
    DRM_FORMAT_YVU420,
#endif
  };

  return base::Contains(kEGLImageDrmFmtsSupported,
                        V4L2PixFmtToDrmFormat(fourcc.ToV4L2PixFmt()));
}

std::vector<uint32_t> V4L2Device::PreferredInputFormat(Type type) const {
  if (type == Type::kEncoder) {
    return {V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV12};
  }

  return {};
}

VideoEncodeAccelerator::SupportedRateControlMode
V4L2Device::GetSupportedRateControlMode() {
  auto rate_control_mode = VideoEncodeAccelerator::kNoMode;
  v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
  if (Ioctl(VIDIOC_QUERYCTRL, &query_ctrl)) {
    DPLOG(WARNING) << "QUERYCTRL for bitrate mode failed";
    return rate_control_mode;
  }

  v4l2_querymenu query_menu;
  memset(&query_menu, 0, sizeof(query_menu));
  query_menu.id = query_ctrl.id;
  for (query_menu.index = query_ctrl.minimum;
       base::checked_cast<int>(query_menu.index) <= query_ctrl.maximum;
       query_menu.index++) {
    if (Ioctl(VIDIOC_QUERYMENU, &query_menu) == 0) {
      switch (query_menu.index) {
        case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
          rate_control_mode |= VideoEncodeAccelerator::kConstantMode;
          break;
        case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
          if (!base::FeatureList::IsEnabled(kChromeOSHWVBREncoding)) {
            DVLOGF(3) << "Skip VBR capability";
            break;
          }
          rate_control_mode |= VideoEncodeAccelerator::kVariableMode;
          break;
        default:
          DVLOGF(4) << "Skip bitrate mode: " << query_menu.index;
          break;
      }
    }
  }

  return rate_control_mode;
}

std::vector<uint32_t> V4L2Device::GetSupportedImageProcessorPixelformats(
    v4l2_buf_type buf_type) {
  std::vector<uint32_t> supported_pixelformats;

  Type type = Type::kImageProcessor;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto pixelformats = EnumerateSupportedPixFmts(
        base::BindRepeating(&V4L2Device::Ioctl, this), buf_type);

    supported_pixelformats.insert(supported_pixelformats.end(),
                                  pixelformats.begin(), pixelformats.end());
    CloseDevice();
  }

  return supported_pixelformats;
}

VideoDecodeAccelerator::SupportedProfiles
V4L2Device::GetSupportedDecodeProfiles(
    const std::vector<uint32_t>& pixelformats) {
  VideoDecodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kDecoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles = EnumerateSupportedDecodeProfiles(pixelformats);
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

VideoEncodeAccelerator::SupportedProfiles
V4L2Device::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles supported_profiles;

  Type type = Type::kEncoder;
  const auto& devices = GetDevicesForType(type);
  for (const auto& device : devices) {
    if (!OpenDevicePath(device.first)) {
      VLOGF(1) << "Failed opening " << device.first;
      continue;
    }

    const auto& profiles = EnumerateSupportedEncodeProfiles();
    supported_profiles.insert(supported_profiles.end(), profiles.begin(),
                              profiles.end());
    CloseDevice();
  }

  return supported_profiles;
}

bool V4L2Device::IsImageProcessingSupported() {
  const auto& devices = GetDevicesForType(Type::kImageProcessor);
  return !devices.empty();
}

bool V4L2Device::IsJpegDecodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegDecoder);
  return !devices.empty();
}

bool V4L2Device::IsJpegEncodingSupported() {
  const auto& devices = GetDevicesForType(Type::kJpegEncoder);
  return !devices.empty();
}

VideoDecodeAccelerator::SupportedProfiles
V4L2Device::EnumerateSupportedDecodeProfiles(
    const std::vector<uint32_t>& pixelformats) {
  VideoDecodeAccelerator::SupportedProfiles profiles;

  const auto v4l2_codecs_as_pix_fmts =
      EnumerateSupportedPixFmts(base::BindRepeating(&V4L2Device::Ioctl, this),
                                V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

  for (uint32_t pixelformat : v4l2_codecs_as_pix_fmts) {
    if (!base::Contains(pixelformats, pixelformat)) {
      continue;
    }

    // Skip AV1 decoder profiles if kChromeOSHWAV1Decoder is disabled.
    if ((pixelformat == V4L2_PIX_FMT_AV1 ||
         pixelformat == V4L2_PIX_FMT_AV1_FRAME) &&
        !base::FeatureList::IsEnabled(kChromeOSHWAV1Decoder)) {
      continue;
    }

    VideoDecodeAccelerator::SupportedProfile profile;
    GetSupportedResolution(base::BindRepeating(&V4L2Device::Ioctl, this),
                           pixelformat, &profile.min_resolution,
                           &profile.max_resolution);

    const auto video_codec_profiles = EnumerateSupportedProfilesForV4L2Codec(
        base::BindRepeating(&V4L2Device::Ioctl, this), pixelformat);

    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;
      profiles.push_back(profile);

      DVLOGF(3) << "Found decoder profile " << GetProfileName(profile.profile)
                << ", resolutions: " << profile.min_resolution.ToString() << " "
                << profile.max_resolution.ToString();
    }
  }

  return profiles;
}

VideoEncodeAccelerator::SupportedProfiles
V4L2Device::EnumerateSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;

  const auto v4l2_codecs_as_pix_fmts =
      EnumerateSupportedPixFmts(base::BindRepeating(&V4L2Device::Ioctl, this),
                                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  for (const auto& pixelformat : v4l2_codecs_as_pix_fmts) {
    VideoEncodeAccelerator::SupportedProfile profile;
    profile.max_framerate_numerator = 30;
    profile.max_framerate_denominator = 1;

    profile.rate_control_modes = GetSupportedRateControlMode();
    if (profile.rate_control_modes == VideoEncodeAccelerator::kNoMode) {
      DLOG(ERROR) << "Skipped because no bitrate mode is supported for "
                  << FourccToString(pixelformat);
      continue;
    }
    gfx::Size min_resolution;
    GetSupportedResolution(base::BindRepeating(&V4L2Device::Ioctl, this),
                           pixelformat, &min_resolution,
                           &profile.max_resolution);
    const auto video_codec_profiles = EnumerateSupportedProfilesForV4L2Codec(
        base::BindRepeating(&V4L2Device::Ioctl, this), pixelformat);

    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;

      profile.scalability_modes = GetSupportedScalabilityModesForV4L2Codec(
          base::BindRepeating(&V4L2Device::Ioctl, this), video_codec_profile);

      profiles.push_back(profile);

      DVLOGF(3) << "Found encoder profile " << GetProfileName(profile.profile)
                << ", max resolution: " << profile.max_resolution.ToString();
    }
  }

  return profiles;
}

bool V4L2Device::StartPolling(V4L2DevicePoller::EventCallback event_callback,
                              base::RepeatingClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!device_poller_) {
    device_poller_ =
        std::make_unique<V4L2DevicePoller>(this, "V4L2DevicePollerThread");
  }

  bool ret = device_poller_->StartPolling(std::move(event_callback),
                                          std::move(error_callback));

  if (!ret)
    device_poller_ = nullptr;

  return ret;
}

bool V4L2Device::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return !device_poller_ || device_poller_->StopPolling();
}

void V4L2Device::SchedulePoll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (!device_poller_ || !device_poller_->IsPolling())
    return;

  device_poller_->SchedulePoll();
}

std::optional<struct v4l2_event> V4L2Device::DequeueEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  struct v4l2_event event;
  memset(&event, 0, sizeof(event));

  if (Ioctl(VIDIOC_DQEVENT, &event) != 0) {
    // The ioctl will fail if there are no pending events. This is part of the
    // normal flow, so keep this log level low.
    VPLOGF(4) << "Failed to dequeue event";
    return std::nullopt;
  }

  return event;
}

V4L2RequestsQueue* V4L2Device::GetRequestsQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (requests_queue_creation_called_)
    return requests_queue_.get();

  requests_queue_creation_called_ = true;

  struct v4l2_capability caps;
  if (Ioctl(VIDIOC_QUERYCAP, &caps)) {
    VPLOGF(1) << "Failed to query device capabilities.";
    return nullptr;
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
    base::ScopedFD candidate_media_fd(
        HANDLE_EINTR(open(path.c_str(), O_RDWR, 0)));
    if (!candidate_media_fd.is_valid()) {
      VPLOGF(2) << "Failed to open media device: " << path;
      continue;
    }

    struct media_device_info media_info;
    if (HANDLE_EINTR(ioctl(candidate_media_fd.get(), MEDIA_IOC_DEVICE_INFO,
                           &media_info)) < 0) {
      RecordMediaIoctlUMA(MediaIoctlRequests::kMediaIocDeviceInfo);
      VPLOGF(2) << "Failed to Query media device info.";
      continue;
    }

    // Match the video device and the media controller by the bus_info
    // field. This works better than the driver field if there are multiple
    // instances of the same decoder driver in the system. However old MediaTek
    // drivers didn't fill in the bus_info field for the media device.
    if (strlen(reinterpret_cast<const char*>(caps.bus_info)) > 0 &&
        strlen(reinterpret_cast<const char*>(media_info.bus_info)) > 0 &&
        strncmp(reinterpret_cast<const char*>(caps.bus_info),
                reinterpret_cast<const char*>(media_info.bus_info),
                sizeof(caps.bus_info))) {
      continue;
    }

    // Fall back to matching the video device and the media controller by the
    // driver field. The mtk-vcodec driver does not fill the card and bus fields
    // properly, so those won't work.
    if (strncmp(reinterpret_cast<const char*>(caps.driver),
                reinterpret_cast<const char*>(media_info.driver),
                sizeof(caps.driver))) {
      continue;
    }

    media_fd = std::move(candidate_media_fd);
    break;
  }

  if (!media_fd.is_valid()) {
    VLOGF(1) << "Failed to open matching media device.";
    return nullptr;
  }

  // Not using std::make_unique because constructor is private.
  std::unique_ptr<V4L2RequestsQueue> requests_queue(
      new V4L2RequestsQueue(std::move(media_fd)));
  requests_queue_ = std::move(requests_queue);

  return requests_queue_.get();
}

bool V4L2Device::IsCtrlExposed(uint32_t ctrl_id) {
  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = ctrl_id;

  return Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) == 0;
}

bool V4L2Device::SetExtCtrls(uint32_t ctrl_class,
                             std::vector<V4L2ExtCtrl> ctrls,
                             V4L2RequestRef* request_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (ctrls.empty())
    return true;

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;
  ext_ctrls.count = 0;
  const bool use_modern_s_ext_ctrls =
      Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) == 0;

  ext_ctrls.which =
      use_modern_s_ext_ctrls ? V4L2_CTRL_WHICH_CUR_VAL : ctrl_class;
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0].ctrl;

  if (request_ref)
    request_ref->ApplyCtrls(&ext_ctrls);

  const int result = Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls);
  if (result < 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    if (ext_ctrls.error_idx == ext_ctrls.count)
      VPLOGF(1) << "VIDIOC_S_EXT_CTRLS: validation failed while trying to set "
                   "controls";
    else
      VPLOGF(1) << "VIDIOC_S_EXT_CTRLS: unable to set control (0x" << std::hex
                << ctrls[ext_ctrls.error_idx].ctrl.id << ") at index ("
                << ext_ctrls.error_idx << ")  to 0x"
                << ctrls[ext_ctrls.error_idx].ctrl.value;
  }

  return result == 0;
}

std::optional<struct v4l2_ext_control> V4L2Device::GetCtrl(uint32_t ctrl_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));

  ctrl.id = ctrl_id;
  ext_ctrls.controls = &ctrl;
  ext_ctrls.count = 1;

  if (Ioctl(VIDIOC_G_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(3) << "Failed to get control";
    return std::nullopt;
  }

  return ctrl;
}

bool V4L2Device::SetGOPLength(uint32_t gop_length) {
  if (!SetExtCtrls(V4L2_CTRL_CLASS_MPEG,
                   {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_GOP_SIZE, gop_length)})) {
    // Some platforms allow setting the GOP length to 0 as
    // a way of turning off keyframe placement.  If the platform
    // does not support turning off periodic keyframe placement,
    // set the GOP to the maximum supported value.
    if (gop_length == 0) {
      v4l2_query_ext_ctrl queryctrl;
      memset(&queryctrl, 0, sizeof(queryctrl));

      queryctrl.id = V4L2_CTRL_CLASS_MPEG | V4L2_CID_MPEG_VIDEO_GOP_SIZE;
      if (Ioctl(VIDIOC_QUERY_EXT_CTRL, &queryctrl) == 0) {
        VPLOGF(3) << "Unable to set GOP to 0, instead using max : "
                  << queryctrl.maximum;
        return SetExtCtrls(
            V4L2_CTRL_CLASS_MPEG,
            {V4L2ExtCtrl(V4L2_CID_MPEG_VIDEO_GOP_SIZE, queryctrl.maximum)});
      }
    }
    return false;
  }
  return true;
}

bool V4L2Device::OpenDevicePath(const std::string& path) {
  DCHECK(!device_fd_.is_valid());

  device_fd_.reset(
      HANDLE_EINTR(open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC)));
  return device_fd_.is_valid();
}

void V4L2Device::CloseDevice() {
  DVLOGF(3);
  device_fd_.reset();
}

void V4L2Device::EnumerateDevicesForType(Type type) {
  static const std::string kDecoderDevicePattern = "/dev/video-dec";
  static const std::string kEncoderDevicePattern = "/dev/video-enc";
  static const std::string kImageProcessorDevicePattern = "/dev/image-proc";
  static const std::string kJpegDecoderDevicePattern = "/dev/jpeg-dec";
  static const std::string kJpegEncoderDevicePattern = "/dev/jpeg-enc";

  std::string device_pattern;
  v4l2_buf_type buf_type;
  switch (type) {
    case Type::kDecoder:
      device_pattern = kDecoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kEncoder:
      device_pattern = kEncoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      break;
    case Type::kImageProcessor:
      device_pattern = kImageProcessorDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kJpegDecoder:
      device_pattern = kJpegDecoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      break;
    case Type::kJpegEncoder:
      device_pattern = kJpegEncoderDevicePattern;
      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      break;
  }

  std::vector<std::string> candidate_paths;

  // TODO(posciak): Remove this legacy unnumbered device once
  // all platforms are updated to use numbered devices.
  candidate_paths.push_back(device_pattern);

  // We are sandboxed, so we can't query directory contents to check which
  // devices are actually available. Try to open the first 10; if not present,
  // we will just fail to open immediately.
  for (int i = 0; i < 10; ++i) {
    candidate_paths.push_back(
        base::StringPrintf("%s%d", device_pattern.c_str(), i));
  }

  Devices devices;
  for (const auto& path : candidate_paths) {
    if (!OpenDevicePath(path)) {
      continue;
    }
    const auto supported_pixelformats = EnumerateSupportedPixFmts(
        base::BindRepeating(&V4L2Device::Ioctl, this), buf_type);

    if (!supported_pixelformats.empty()) {
      DVLOGF(3) << "Found device: " << path;
      devices.push_back(std::make_pair(path, supported_pixelformats));
    }

    CloseDevice();
  }

  DCHECK_EQ(devices_by_type_.count(type), 0u);
  devices_by_type_[type] = devices;
}

const V4L2Device::Devices& V4L2Device::GetDevicesForType(Type type) {
  if (devices_by_type_.count(type) == 0) {
    EnumerateDevicesForType(type);
  }

  DCHECK_NE(devices_by_type_.count(type), 0u);
  return devices_by_type_[type];
}

std::string V4L2Device::GetDevicePathFor(Type type, uint32_t pixfmt) {
  const Devices& devices = GetDevicesForType(type);

  for (const auto& device : devices) {
    if (base::Contains(device.second, pixfmt)) {
      return device.first;
    }
  }

  return std::string();
}

}  //  namespace media
