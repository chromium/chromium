// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_webrtc.h"

#include "base/feature_list.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/linux/video_capture_device_webrtc.h"
#include "third_party/webrtc/modules/video_capture/video_capture_factory.h"

namespace media {

struct {
  webrtc::VideoType video_type;
  VideoPixelFormat pixel_format;
} constexpr kSupportedFormats[] = {
    {webrtc::VideoType::kUnknown, PIXEL_FORMAT_UNKNOWN},
    {webrtc::VideoType::kI420, PIXEL_FORMAT_I420},
    {webrtc::VideoType::kRGB24, PIXEL_FORMAT_RGB24},
    {webrtc::VideoType::kARGB, PIXEL_FORMAT_ARGB},
    {webrtc::VideoType::kYUY2, PIXEL_FORMAT_YUY2},
    {webrtc::VideoType::kNV12, PIXEL_FORMAT_NV12},
    {webrtc::VideoType::kUYVY, PIXEL_FORMAT_UYVY},
    {webrtc::VideoType::kMJPEG, PIXEL_FORMAT_MJPEG},
    {webrtc::VideoType::kBGRA, PIXEL_FORMAT_BGRA},
};

VideoCaptureDeviceFactoryWebRtc::VideoCaptureDeviceFactoryWebRtc() = default;

VideoCaptureDeviceFactoryWebRtc::~VideoCaptureDeviceFactoryWebRtc() = default;

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryWebRtc::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(status_, webrtc::VideoCaptureOptions::Status::SUCCESS);
  CHECK(options_);
  return VideoCaptureDeviceWebRtc::Create(options_.get(), device_descriptor);
}

void VideoCaptureDeviceFactoryWebRtc::FinishGetDevicesInfo() {
  std::vector<VideoCaptureDeviceInfo> devices_info;

  if (options_) {
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo(options_.get()));

    if (info) {
      for (uint32_t i = 0; i < info->NumberOfDevices(); ++i) {
        char device_name[webrtc::kVideoCaptureDeviceNameLength];
        char unique_name[webrtc::kVideoCaptureUniqueNameLength];
        char product_id[webrtc::kVideoCaptureProductIdLength];

        if (info->GetDeviceName(
                i, device_name, webrtc::kVideoCaptureDeviceNameLength,
                unique_name, webrtc::kVideoCaptureUniqueNameLength, product_id,
                webrtc::kVideoCaptureProductIdLength) < 0) {
          continue;
        }

        VLOG(1) << "Found Camera: " << device_name << " (" << product_id << ")";

        VideoFacingMode facing_mode = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
        VideoCaptureControlSupport control_support;
        VideoCaptureDeviceInfo device_info(VideoCaptureDeviceDescriptor(
            device_name, unique_name, product_id,
            VideoCaptureApi::WEBRTC_LINUX_PIPEWIRE_SINGLE_PLANE,
            control_support, VideoCaptureTransportType::OTHER_TRANSPORT,
            facing_mode));

        int num_capabilities = info->NumberOfCapabilities(unique_name);
        for (int j = 0; j < num_capabilities; ++j) {
          webrtc::VideoCaptureCapability capability;

          if (info->GetCapability(unique_name, j, capability) < 0) {
            continue;
          }
          if (capability.interlaced) {
            continue;
          }

          VideoCaptureFormat supported_format;
          supported_format.pixel_format =
              WebRtcVideoTypeToChromiumPixelFormat(capability.videoType);
          supported_format.frame_rate = capability.maxFPS;
          supported_format.frame_size.SetSize(capability.width,
                                              capability.height);
          device_info.supported_formats.push_back(supported_format);
          VLOG(2) << "Found Format: "
                  << VideoCaptureFormat::ToString(supported_format);
        }
        devices_info.push_back(device_info);
      }
    }
  }

  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), devices_info));
}

void VideoCaptureDeviceFactoryWebRtc::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  origin_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  callback_ = std::move(callback);

  if (status_ != webrtc::VideoCaptureOptions::Status::UNINITIALIZED) {
    FinishGetDevicesInfo();
    return;
  }

  options_ = std::make_unique<webrtc::VideoCaptureOptions>();
#if defined(WEBRTC_USE_PIPEWIRE)
  if (base::FeatureList::IsEnabled(features::kWebRtcPipeWireCamera)) {
    options_->set_allow_pipewire(true);
  }
#endif
  options_->Init(this);
}

void VideoCaptureDeviceFactoryWebRtc::OnInitialized(
    webrtc::VideoCaptureOptions::Status status) {
  if (status != webrtc::VideoCaptureOptions::Status::SUCCESS) {
    options_.reset();
  }

  status_ = status;
  FinishGetDevicesInfo();
}

bool VideoCaptureDeviceFactoryWebRtc::IsAvailable() {
  return status_ != webrtc::VideoCaptureOptions::Status::UNAVAILABLE;
}

webrtc::VideoType
VideoCaptureDeviceFactoryWebRtc::WebRtcVideoTypeFromChromiumPixelFormat(
    VideoPixelFormat pixel_format) {
  for (const auto& type_and_format : kSupportedFormats) {
    if (type_and_format.pixel_format == pixel_format) {
      return type_and_format.video_type;
    }
  }
  return webrtc::VideoType::kUnknown;
}

VideoPixelFormat
VideoCaptureDeviceFactoryWebRtc::WebRtcVideoTypeToChromiumPixelFormat(
    webrtc::VideoType video_type) {
  for (const auto& type_and_format : kSupportedFormats) {
    if (type_and_format.video_type == video_type) {
      return type_and_format.pixel_format;
    }
  }
  return PIXEL_FORMAT_UNKNOWN;
}

}  // namespace media
