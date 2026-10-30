// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <functional>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/capture/video/linux/scoped_v4l2_device_fd.h"
#include "media/capture/video/linux/v4l2_capture_device.h"
#include "media/capture/video/linux/v4l2_capture_device_impl.h"
#include "media/capture/video/linux/video_capture_device_linux.h"

#if BUILDFLAG(IS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace media {

namespace {

bool CompareCaptureDevices(const VideoCaptureDeviceInfo& a,
                           const VideoCaptureDeviceInfo& b) {
  return a.descriptor < b.descriptor;
}

// USB VID and PID are both 4 bytes long.
const size_t kVidPidSize = 4;
const size_t kMaxInterfaceNameSize = 256;

// /sys/class/video4linux/video{N}/device is a symlink to the corresponding
// USB device info directory.
const char kVidPathTemplate[] = "/sys/class/video4linux/%s/device/../idVendor";
const char kPidPathTemplate[] = "/sys/class/video4linux/%s/device/../idProduct";
const char kInterfacePathTemplate[] =
    "/sys/class/video4linux/%s/device/interface";

// Common sizes tried inside a stepwise or continuous frame size range.
constexpr gfx::Size kCommonFrameSizes[] = {
    {3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 1200}, {1280, 960},
    {1280, 720},  {1024, 768},  {800, 600},   {640, 480},   {640, 360},
    {352, 288},   {320, 240},   {320, 180},   {160, 120}};

bool IsInStepwiseRange(const v4l2_frmsize_stepwise& stepwise,
                       const gfx::Size& size) {
  const uint32_t width = size.width();
  const uint32_t height = size.height();
  if (width < stepwise.min_width || width > stepwise.max_width ||
      height < stepwise.min_height || height > stepwise.max_height) {
    return false;
  }
  // A continuous range has a step of 1.
  const uint32_t step_width = std::max(stepwise.step_width, 1u);
  const uint32_t step_height = std::max(stepwise.step_height, 1u);
  return (width - stepwise.min_width) % step_width == 0 &&
         (height - stepwise.min_height) % step_height == 0;
}

bool ReadIdFile(const std::string& path, std::string* id) {
  std::string content;
  if (!base::ReadFileToString(base::FilePath(path), &content)) {
    return false;
  }
  if (content.length() < kVidPidSize) {
    return false;
  }
  id->append(content, 0, kVidPidSize);
  return true;
}

std::string ExtractFileNameFromDeviceId(const std::string& device_id) {
  // |unique_id| is of the form "/dev/video2".  |file_name| is "video2".
  constexpr std::string_view kDevDir = "/dev/";
  DCHECK(device_id.starts_with(kDevDir));
  return device_id.substr(kDevDir.length());
}

class DevVideoFilePathsDeviceProvider
    : public VideoCaptureDeviceFactoryV4L2::DeviceProvider {
 public:
  void GetDeviceIds(std::vector<std::string>* target_container) override {
    const base::FilePath path("/dev/");
    base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES,
                                    "video*");
    while (!enumerator.Next().empty()) {
      const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
      target_container->emplace_back(path.value() + info.GetName().value());
    }
  }

  std::string GetDeviceModelId(const std::string& device_id) override {
    const std::string file_name = ExtractFileNameFromDeviceId(device_id);
    std::string usb_id;
    const std::string vid_path =
        base::StringPrintf(kVidPathTemplate, file_name.c_str());
    if (!ReadIdFile(vid_path, &usb_id)) {
      return usb_id;
    }

    usb_id.append(":");
    const std::string pid_path =
        base::StringPrintf(kPidPathTemplate, file_name.c_str());
    if (!ReadIdFile(pid_path, &usb_id)) {
      usb_id.clear();
    }

    return usb_id;
  }

  std::string GetDeviceDisplayName(const std::string& device_id) override {
    const std::string file_name = ExtractFileNameFromDeviceId(device_id);
    const std::string interface_path =
        base::StringPrintf(kInterfacePathTemplate, file_name.c_str());
    std::string display_name;
    if (!base::ReadFileToStringWithMaxSize(base::FilePath(interface_path),
                                           &display_name,
                                           kMaxInterfaceNameSize)) {
      return std::string();
    }
    return display_name;
  }
};

}  // namespace

VideoCaptureDeviceFactoryV4L2::VideoCaptureDeviceFactoryV4L2(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : v4l2_(base::MakeRefCounted<V4L2CaptureDeviceImpl>()),
      device_provider_(std::make_unique<DevVideoFilePathsDeviceProvider>()),
      ui_task_runner_(ui_task_runner) {}

VideoCaptureDeviceFactoryV4L2::~VideoCaptureDeviceFactoryV4L2() = default;

void VideoCaptureDeviceFactoryV4L2::SetV4L2EnvironmentForTesting(
    scoped_refptr<V4L2CaptureDevice> v4l2,
    std::unique_ptr<VideoCaptureDeviceFactoryV4L2::DeviceProvider>
        device_provider) {
  v4l2_ = std::move(v4l2);
  device_provider_ = std::move(device_provider);
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryV4L2::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto self =
      std::make_unique<VideoCaptureDeviceLinux>(v4l2_.get(), device_descriptor);

  // Test opening the device driver. This is to make sure it is available.
  // We will reopen it again in our worker thread when someone
  // allocates the camera.
  ScopedV4L2DeviceFD fd(
      v4l2_.get(),
      HANDLE_EINTR(v4l2_->open(device_descriptor.device_id.c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DLOG(ERROR) << "Cannot open device";
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::kV4L2FailedToOpenV4L2DeviceDriverFile);
  }

  return VideoCaptureErrorOrDevice(std::move(self));
}

void VideoCaptureDeviceFactoryV4L2::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<VideoCaptureDeviceInfo> devices_info;
  std::vector<std::string> filepaths;
  device_provider_->GetDeviceIds(&filepaths);
  for (auto& unique_id : filepaths) {
    const ScopedV4L2DeviceFD fd(
        v4l2_.get(), HANDLE_EINTR(v4l2_->open(unique_id.c_str(), O_RDONLY)));
    if (!fd.is_valid()) {
      DLOG(ERROR) << "Couldn't open " << unique_id;
      continue;
    }
    // Test if this is a V4L2CaptureDevice capture device and if it has at least
    // one supported capture format. Devices that have capture and output
    // capabilities at the same time are memory-to-memory and are skipped, see
    // http://crbug.com/139356.
    // In theory, checking for CAPTURE/OUTPUT in caps.capabilities should only
    // be done if V4L2_CAP_DEVICE_CAPS is not set. However, this was not done
    // in the past and it is unclear if it breaks with existing devices. And if
    // a device is accepted incorrectly then it will not have any usable
    // formats and is skipped anyways.
    v4l2_capability cap;
    if ((DoIoctl(fd.get(), VIDIOC_QUERYCAP, &cap) == 0) &&
        ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE &&
          !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) ||
         (cap.capabilities & V4L2_CAP_DEVICE_CAPS &&
          cap.device_caps & V4L2_CAP_VIDEO_CAPTURE &&
          !(cap.device_caps & V4L2_CAP_VIDEO_OUTPUT))) &&
        HasUsableFormats(fd.get(), cap.capabilities)) {
      const std::string model_id =
          device_provider_->GetDeviceModelId(unique_id);
      std::string display_name =
          device_provider_->GetDeviceDisplayName(unique_id);
      if (display_name.empty()) {
        display_name = reinterpret_cast<char*>(cap.card);
      }

      VideoFacingMode facing_mode = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;

      VideoCaptureFormats supported_formats;
      GetSupportedFormatsForV4L2BufferType(fd.get(), &supported_formats);
      if (supported_formats.empty()) {
        DVLOG(1) << "No supported formats: " << unique_id;
        continue;
      }

      devices_info.emplace_back(VideoCaptureDeviceDescriptor(
          display_name, unique_id, model_id,
          VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE, GetControlSupport(fd.get()),
          VideoCaptureTransportType::OTHER_TRANSPORT, facing_mode));

      devices_info.back().supported_formats = std::move(supported_formats);
    }
  }

  // This is required for some applications that rely on the stable ordering of
  // devices.
  std::sort(devices_info.begin(), devices_info.end(), CompareCaptureDevices);

  std::move(callback).Run(std::move(devices_info));
}

int VideoCaptureDeviceFactoryV4L2::DoIoctl(int fd, int request, void* argp) {
  return HANDLE_EINTR(v4l2_->ioctl(fd, request, argp));
}

// Check if the video capture device supports pan, tilt and zoom controls.
VideoCaptureControlSupport VideoCaptureDeviceFactoryV4L2::GetControlSupport(
    int fd) {
  VideoCaptureControlSupport control_support;
  control_support.pan = GetControlSupport(fd, V4L2_CID_PAN_ABSOLUTE);
  control_support.tilt = GetControlSupport(fd, V4L2_CID_TILT_ABSOLUTE);
  control_support.zoom = GetControlSupport(fd, V4L2_CID_ZOOM_ABSOLUTE);
  return control_support;
}

bool VideoCaptureDeviceFactoryV4L2::GetControlSupport(int fd, int control_id) {
  v4l2_queryctrl range = {};
  range.id = control_id;
  range.type = V4L2_CTRL_TYPE_INTEGER;
  return DoIoctl(fd, VIDIOC_QUERYCTRL, &range) == 0 &&
         range.minimum < range.maximum;
}

bool VideoCaptureDeviceFactoryV4L2::HasUsableFormats(int fd,
                                                     uint32_t capabilities) {
  if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    return false;
  }

  const std::vector<uint32_t>& usable_fourccs =
      VideoCaptureDeviceLinux::GetListOfUsableFourCCs(false);
  v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (; DoIoctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0; ++fmtdesc.index) {
    if (std::ranges::contains(usable_fourccs, fmtdesc.pixelformat)) {
      return true;
    }
  }

  DVLOG(1) << "No usable formats found";
  return false;
}

std::vector<float> VideoCaptureDeviceFactoryV4L2::GetFrameRateList(
    int fd,
    uint32_t fourcc,
    uint32_t width,
    uint32_t height) {
  std::vector<float> frame_rates;

  v4l2_frmivalenum frame_interval = {};
  frame_interval.pixel_format = fourcc;
  frame_interval.width = width;
  frame_interval.height = height;
  for (; DoIoctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0;
       ++frame_interval.index) {
    if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
      if (frame_interval.discrete.numerator != 0) {
        frame_rates.push_back(
            frame_interval.discrete.denominator /
            static_cast<float>(frame_interval.discrete.numerator));
      }
    } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS ||
               frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
      // TODO(mcasas): see http://crbug.com/249953, support these devices.
      NOTIMPLEMENTED_LOG_ONCE();
      break;
    }
  }
  // Some devices, e.g. Kinect, do not enumerate any frame rates, see
  // http://crbug.com/412284. Set their frame_rate to zero.
  if (frame_rates.empty()) {
    frame_rates.push_back(0);
  }
  return frame_rates;
}

bool VideoCaptureDeviceFactoryV4L2::DriverAcceptsFrameSize(
    int fd,
    uint32_t fourcc,
    const gfx::Size& size) {
  v4l2_format format = {};
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = fourcc;
  format.fmt.pix.width = size.width();
  format.fmt.pix.height = size.height();
  format.fmt.pix.field = V4L2_FIELD_ANY;
  if (DoIoctl(fd, VIDIOC_TRY_FMT, &format) < 0) {
    // VIDIOC_TRY_FMT is optional; a driver without it cannot be asked, so the
    // size is taken as it is.
    return errno == ENOTTY;
  }
  return format.fmt.pix.pixelformat == fourcc &&
         format.fmt.pix.width == static_cast<uint32_t>(size.width()) &&
         format.fmt.pix.height == static_cast<uint32_t>(size.height());
}

std::vector<gfx::Size> VideoCaptureDeviceFactoryV4L2::GetStepwiseFrameSizes(
    int fd,
    uint32_t fourcc,
    const v4l2_frmsize_stepwise& stepwise) {
  std::vector<gfx::Size> candidates;
#if defined(V4L2_SEL_TGT_CROP_BOUNDS)
  // The crop bounds, if the driver reports them.
  v4l2_selection bounds = {};
  bounds.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bounds.target = V4L2_SEL_TGT_CROP_BOUNDS;
  if (DoIoctl(fd, VIDIOC_G_SELECTION, &bounds) == 0) {
    candidates.emplace_back(bounds.r.width, bounds.r.height);
  }
#endif
  // The size the driver currently produces.
  v4l2_format current = {};
  current.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (DoIoctl(fd, VIDIOC_G_FMT, &current) == 0) {
    candidates.emplace_back(current.fmt.pix.width, current.fmt.pix.height);
  }
  candidates.emplace_back(stepwise.max_width, stepwise.max_height);
  candidates.insert(candidates.end(), std::begin(kCommonFrameSizes),
                    std::end(kCommonFrameSizes));
  // Sort largest first and drop duplicates, so that the loop below checks
  // each candidate once.
  std::ranges::sort(candidates, std::ranges::greater(),
                    [](const gfx::Size& size) {
                      return std::pair(size.width(), size.height());
                    });
  const auto duplicates = std::ranges::unique(candidates);
  candidates.erase(duplicates.begin(), duplicates.end());

  std::vector<gfx::Size> sizes;
  for (const gfx::Size& size : candidates) {
    if (IsInStepwiseRange(stepwise, size) &&
        DriverAcceptsFrameSize(fd, fourcc, size)) {
      sizes.push_back(size);
    }
  }
  // None of the sizes above lies on the range's grid, e.g. one starting at an
  // odd width whose maximum is off the grid: fall back to the minimum, which
  // always lies on it.
  const gfx::Size minimum(stepwise.min_width, stepwise.min_height);
  if (sizes.empty() && DriverAcceptsFrameSize(fd, fourcc, minimum)) {
    sizes.push_back(minimum);
  }
  return sizes;
}

void VideoCaptureDeviceFactoryV4L2::GetSupportedFormatsForV4L2BufferType(
    int fd,
    VideoCaptureFormats* supported_formats) {
  v4l2_fmtdesc v4l2_format = {};
  v4l2_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (; DoIoctl(fd, VIDIOC_ENUM_FMT, &v4l2_format) == 0; ++v4l2_format.index) {
    VideoCaptureFormat supported_format;
    supported_format.pixel_format =
        VideoCaptureDeviceLinux::V4l2FourCcToChromiumPixelFormat(
            v4l2_format.pixelformat);

    if (supported_format.pixel_format == PIXEL_FORMAT_UNKNOWN) {
      continue;
    }

    v4l2_frmsizeenum frame_size = {};
    frame_size.pixel_format = v4l2_format.pixelformat;
    for (; DoIoctl(fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0;
         ++frame_size.index) {
      std::vector<gfx::Size> frame_sizes;
      if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        frame_sizes.emplace_back(frame_size.discrete.width,
                                 frame_size.discrete.height);
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                 frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        frame_sizes = GetStepwiseFrameSizes(fd, v4l2_format.pixelformat,
                                            frame_size.stepwise);
      }

      for (const gfx::Size& size : frame_sizes) {
        supported_format.frame_size = size;
        const std::vector<float> frame_rates = GetFrameRateList(
            fd, v4l2_format.pixelformat, size.width(), size.height());
        for (const auto& frame_rate : frame_rates) {
          supported_format.frame_rate = frame_rate;
          supported_formats->push_back(supported_format);
          DVLOG(1) << VideoCaptureFormat::ToString(supported_format);
        }
      }
      // Only index 0 is valid for a stepwise or continuous range.
      if (frame_size.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
        break;
      }
    }
  }
}

}  // namespace media
